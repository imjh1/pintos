#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    block_sector_t double_indirect;     /* double_indirect_block */
    bool is_dir;			/* directory or file */
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    uint32_t unused[124];               /* Not used. */
  };

struct indirect
  {
    block_sector_t block[128];	// 512 Byte (BLOCK SECTOR size)
  };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
  };

bool double_indirect_block_allocate (struct inode_disk *disk_inode, size_t sectors);	/* double indirect block allocation */
void double_indirect_block_deallocate (struct inode *inode);				/* double indirect block deallocation */

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  if (pos < inode->data.length){
    /* continuous block alloc 방식에서 offset에 해당하는 sector 찾는 방식
    -> double indirect block alloc 방식에서 offset에 해당하는 sector 찾는 방식 */
    off_t double_idx = (pos / BLOCK_SECTOR_SIZE) / 128;
    off_t single_idx = (pos / BLOCK_SECTOR_SIZE) % 128;
    struct indirect ind_block;

    block_read (fs_device, inode->data.double_indirect, &ind_block);
    block_read (fs_device, ind_block.block[double_idx], &ind_block);
    return ind_block.block[single_idx];
  }
  else
    return -1;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;
char buf[BLOCK_SECTOR_SIZE];	// to fill with zero when block sector allocated

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
  memset (buf, 0, BLOCK_SECTOR_SIZE);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length, bool is_dir)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      size_t sectors = bytes_to_sectors (length);
      disk_inode->is_dir = is_dir;
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;

      /* continuous block allocation 방식
      -> double indirect block allocation 방식 */    
      if(double_indirect_block_allocate (disk_inode, sectors))
        {
	  block_write (fs_device, sector, disk_inode);
          success = true; 
        } 
      free (disk_inode);
    }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  block_read (fs_device, inode->sector, &inode->data);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
	
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
	  /* continuous block alloc 방식의 deallocation
          -> double indirect block alloc 방식의 deallocation */
          free_map_release (inode->sector, 1);
          double_indirect_block_deallocate (inode);
        }

      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      /* Advance. */
      /* Read from disk 방식
      -> Read from Buffer Cache 방식 */
      buffer_cache_read (sector_idx, buffer, chunk_size, bytes_read, sector_ofs);

      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  /* 기존 file의 크기를 초과하는 위치에 write하는 경우 file extension */
  if (inode_length (inode) < offset + size) {
    if(!double_indirect_block_allocate (&inode->data, bytes_to_sectors (offset + size)))
      return 0;
    inode->data.length = offset + size;
    block_write (fs_device, inode->sector, &inode->data); 
  }

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      /* Advance. */
      /* Write to disk 방식
      -> Write to Buffer Cache 방식 */
      buffer_cache_write (sector_idx, buffer, chunk_size, bytes_written, sector_ofs);

      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}

/* dir 여부 return */
bool inode_is_dir (const struct inode *inode)
{
  return inode->data.is_dir;
}

/* inode remove 여부 return */
bool inode_is_remove (const struct inode *inode)
{
  return inode->removed;
}

int inode_open_cnt (const struct inode *inode)
{
  return inode->open_cnt;
}

/* double indirect block allocation */
bool double_indirect_block_allocate (struct inode_disk *disk_inode, size_t sectors)
{
  if(!disk_inode->double_indirect){
    if(!free_map_allocate (1, &disk_inode->double_indirect))
      return false;
    /* fill with zero */
    block_write (fs_device, disk_inode->double_indirect, buf);
  }

  struct indirect double_indirect;	// double indirect block
  block_read (fs_device, disk_inode->double_indirect, &double_indirect);

  int mod_sector = sectors % 128;
  int double_idx = DIV_ROUND_UP(sectors, 128);
  int single_idx = 128;
 
  for(int i=0; i<double_idx; i++){
    if(i == (double_idx - 1))
      single_idx = mod_sector ? mod_sector : 128;
    
    if(!double_indirect.block[i]){
      if(!free_map_allocate (1, &double_indirect.block[i]))
        return false;
      /* fill with zero */
      block_write (fs_device, double_indirect.block[i], buf);
    }

    struct indirect single_indirect;	// single indirect block
    block_read (fs_device, double_indirect.block[i], &single_indirect);

    for(int j=0; j < single_idx; j++){
      if(!single_indirect.block[j]){	// data block
        if(!free_map_allocate (1, &single_indirect.block[j]))
          return false;
        /* fill with zero */
	block_write (fs_device, single_indirect.block[j], buf);
      }
    }
    /* single indirect block 저장 */
    block_write (fs_device, double_indirect.block[i], &single_indirect);
  }
  /* double indirect block 저장 */
  block_write (fs_device, disk_inode->double_indirect, &double_indirect);
  return true;
}

void double_indirect_block_deallocate (struct inode *inode)
{
  off_t length = inode->data.length;
  size_t sectors = bytes_to_sectors (length);

  int mod_sector = sectors % 128;
  int double_idx = DIV_ROUND_UP (sectors, 128);
  int single_idx = 128;

  struct indirect double_indirect;	// double indirect block
  block_read (fs_device, inode->data.double_indirect, &double_indirect); 

  for(int i=0; i < double_idx; i++){
    if(i == (double_idx - 1))
      single_idx = mod_sector ? mod_sector : 128;

    struct indirect single_indirect;	// single indirect block
    block_read (fs_device, double_indirect.block[i], &single_indirect);
 
    /* data block free하고 disk에 write */  
    for(int j=0; j < single_idx; j++)
      free_map_release(single_indirect.block[j], 1);
    /* single indirect block free하고 disk에 write */
    free_map_release(double_indirect.block[i], 1);
  } 
  /* double indirect block free하고 disk에 write */
  free_map_release(inode->data.double_indirect, 1);
}
