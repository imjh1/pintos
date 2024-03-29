#include "filesys/directory.h"
#include <stdio.h>
#include <string.h>
#include <list.h>
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/thread.h"
#include "threads/malloc.h"

/* A directory. */
struct dir 
  {
    struct inode *inode;                /* Backing store. */
    off_t pos;                          /* Current position. */
    struct dir *par;			/* Parent directory ( ".." 처리 ) */
  };

/* A single directory entry. */
struct dir_entry 
  {
    block_sector_t inode_sector;        /* Sector number of header. */
    char name[NAME_MAX + 1];            /* Null terminated file name. */
    bool in_use;                        /* In use or free? */
  };

/* Creates a directory with space for ENTRY_CNT entries in the
   given SECTOR.  Returns true if successful, false on failure. */
bool
dir_create (block_sector_t sector, size_t entry_cnt)
{
  return inode_create (sector, entry_cnt * sizeof (struct dir_entry), true);
}

/* Opens and returns the directory for the given INODE, of which
   it takes ownership.  Returns a null pointer on failure. */
struct dir *
dir_open (struct dir *par, struct inode *inode) 
{
  struct dir *dir = calloc (1, sizeof *dir);
  if (inode != NULL && dir != NULL)
    {
      dir->inode = inode;
      dir->pos = 0;
      /* root directory */
      if(par == NULL)
        par = dir;
      /* parent directory 정보 저장 */
      dir->par = par;
      return dir;
    }
  else
    {
      inode_close (inode);
      free (dir);
      return NULL; 
    }
}

/* Opens the root directory and returns a directory for it.
   Return true if successful, false on failure. */
struct dir *
dir_open_root (void)
{
  return dir_open (NULL, inode_open (ROOT_DIR_SECTOR));
}

/* Opens and returns a new directory for the same inode as DIR.
   Returns a null pointer on failure. */
struct dir *
dir_reopen (struct dir *dir) 
{
  return dir_open (dir->par, inode_reopen (dir->inode));
}

/* Destroys DIR and frees associated resources. */
void
dir_close (struct dir *dir) 
{
  if (dir != NULL)
    {
      inode_close (dir->inode);
      free (dir);
    }
}

/* Returns the inode encapsulated by DIR. */
struct inode *
dir_get_inode (struct dir *dir) 
{
  return dir->inode;
}

/* Searches DIR for a file with the given NAME.
   If successful, returns true, sets *EP to the directory entry
   if EP is non-null, and sets *OFSP to the byte offset of the
   directory entry if OFSP is non-null.
   otherwise, returns false and ignores EP and OFSP. */
static bool
lookup (const struct dir *dir, const char *name,
        struct dir_entry *ep, off_t *ofsp) 
{
  struct dir_entry e;
  size_t ofs;
  
  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) 
    if (e.in_use && !strcmp (name, e.name)) 
      {
        if (ep != NULL)
          *ep = e;
        if (ofsp != NULL)
          *ofsp = ofs;
        return true;
      }
  return false;
}

/* Searches DIR for a file with the given NAME
   and returns true if one exists, false otherwise.
   On success, sets *INODE to an inode for the file, otherwise to
   a null pointer.  The caller must close *INODE. */
bool
dir_lookup (const struct dir *dir, const char *name,
            struct inode **inode) 
{
  struct dir_entry e;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* current directory */
  if (!strcmp (name, "."))
    *inode = inode_reopen (dir->inode);
  /* parent directory */
  else if (!strcmp (name, "..")){
  /* root directory의 parent는 자기 자신 */
    if (inode_get_inumber (dir->inode) == ROOT_DIR_SECTOR)
      *inode = inode_reopen (dir->inode);    
    else 
      *inode = inode_reopen (dir->par->inode);
  }
  else if (lookup (dir, name, &e, NULL))
    *inode = inode_open (e.inode_sector);
  else
    *inode = NULL;

  return *inode != NULL;
}

/* Adds a file named NAME to DIR, which must not already contain a
   file by that name.  The file's inode is in sector
   INODE_SECTOR.
   Returns true if successful, false on failure.
   Fails if NAME is invalid (i.e. too long) or a disk or memory
   error occurs. */
bool
dir_add (struct dir *dir, const char *name, block_sector_t inode_sector)
{
  struct dir_entry e;
  off_t ofs;
  bool success = false;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Check NAME for validity. */
  if (*name == '\0' || strlen (name) > NAME_MAX)
    return false;

  /* Check that NAME is not in use. */
  if (lookup (dir, name, NULL, NULL))
    goto done;

  /* 새로 추가하는 entry가 directory인 경우 */
  struct inode *new_entry = inode_open (inode_sector);
  if (inode_is_dir (new_entry)) {
    struct dir *subdir = dir_open (dir, new_entry);
    dir_close (subdir); 
  }
  inode_close (new_entry);

  /* Set OFS to offset of free slot.
     If there are no free slots, then it will be set to the
     current end-of-file.
     
     inode_read_at() will only return a short read at end of file.
     Otherwise, we'd need to verify that we didn't get a short
     read due to something intermittent such as low memory. */
  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) 
    if (!e.in_use)
      break;

  /* Write slot. */
  e.in_use = true;
  strlcpy (e.name, name, sizeof e.name);
  e.inode_sector = inode_sector;
  success = inode_write_at (dir->inode, &e, sizeof e, ofs) == sizeof e;

 done:
  return success;
}

/* Removes any entry for NAME in DIR.
   Returns true if successful, false on failure,
   which occurs only if there is no file with the given NAME. */
bool
dir_remove (struct dir *dir, const char *name) 
{
  struct dir_entry e;
  struct inode *inode = NULL;
  bool success = false;
  off_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Find directory entry. */
  if (!lookup (dir, name, &e, &ofs))
    goto done;

  /* Open inode. */
  inode = inode_open (e.inode_sector);
  if (inode == NULL)
    goto done;

  /* 지우려는 entry가 directory인 경우 empty check */
  if (inode_is_dir (inode)) {
    struct dir *d = dir_open (dir, inode);
    struct dir_entry de;
    off_t pos = d->pos;
  
    d->pos = 0;
    while (inode_read_at (d->inode, &de, sizeof de, d->pos) == sizeof de)
      {
        d->pos += sizeof de;
	/* directory가 non-empty인 경우 삭제x */
        if (de.in_use)
          {
	    d->pos = pos;
	    dir_close (d);
            goto done;
          }
      }
    d->pos = pos;
    dir_close (d);
  }

  /* Erase directory entry. */
  e.in_use = false;
  if (inode_write_at (dir->inode, &e, sizeof e, ofs) != sizeof e) 
    goto done;

  /* Remove inode. */
  inode_remove (inode);
  success = true;

 done:
  inode_close (inode);
  return success;
}

/* Reads the next directory entry in DIR and stores the name in
   NAME.  Returns true if successful, false if the directory
   contains no more entries. */
bool
dir_readdir (struct dir *dir, char name[NAME_MAX + 1])
{
  struct dir_entry e;

  while (inode_read_at (dir->inode, &e, sizeof e, dir->pos) == sizeof e) 
    {
      dir->pos += sizeof e;
      if (e.in_use)
        {
          strlcpy (name, e.name, NAME_MAX + 1);
          return true;
        } 
    }
  return false;
}

/* "/" 기준으로 "name"을 subdirectory인 "directory"와 파일이름 "file"로 parsing */
void parse_path (char *name, char *directory, char *file)
{
  char str[NAME_MAX + 1];
  strlcpy(str, name, strlen(name) + 1);
  char *next_ptr;
  char *ptr = strtok_r(str, "/", &next_ptr);

  /* absolute path */
  if(name[0] == '/')
    strlcpy (directory, "/", 2);
  /* relative path */
  else
    strlcpy (directory, "", 1);
  strlcpy (file, "", 1);

  while (ptr){
    if(strlen (file)){
      strlcat (directory, file, strlen (directory) + strlen (file) + 1);
      strlcat (directory, "/", strlen (directory) + 2);
    }

    strlcpy (file, ptr, strlen (ptr) + 1);
    ptr = strtok_r (NULL, "/", &next_ptr);
  }
}

/* parsing한 directory path를 찾아가 해당 directory return */
struct dir *open_directory_path (char *directory)
{
  struct dir *dir;
  /* absolute path거나, current working directory init안된 경우 root부터 시작 */
  if (directory[0] == '/' || thread_current ()->cur_dir == NULL)
    dir = dir_open_root ();
  /* relative path인 경우 current working directory부터 시작 */
  else
    dir = dir_reopen (thread_current ()->cur_dir); 

  char *next_ptr;
  char *ptr = strtok_r (directory, "/", &next_ptr); 
  /* "/" 기준으로 directory 구분하면서 찾아감 */
  while(ptr){
    /* current directory */
    if (!strcmp (ptr, ".")){
      ptr = strtok_r (NULL, "/", &next_ptr);
      continue;
    }
    /* parent directory */
    if (!strcmp (ptr, "..")){
      struct dir *next_dir = dir_reopen (dir->par);
      dir_close (dir);
      dir = next_dir;
      ptr = strtok_r (NULL, "/", &next_ptr);
      continue;
    }

    struct inode *disk_inode;

    /* "dir" 디렉터리에 "ptr"이라는 entry file(directory) 존재하는지 check */ 
    if (!dir_lookup (dir, ptr, &disk_inode)){
      dir_close (dir);
      return NULL;
    }

    /* 존재할 경우 해당 entry가 directory인지 check */
    if (!inode_is_dir (disk_inode))
      return NULL; 
    
    /* 해당 directory 새로 open 
       "dir"를 새로 open한 directory로 변경하고,
       "/" 기준으로 문자열 다시 parsing하여 위 과정 반복 */
    struct dir *next_dir = dir_open (dir, disk_inode);
//    dir_close (dir);
    dir = next_dir;
    ptr = strtok_r (NULL, "/", &next_ptr);
  } 

  /* path를 따라 open한 directory가 이미 remove된 경우 */
  if (inode_is_remove (dir_get_inode(dir))) {
    dir_close(dir);
    return NULL;
  }

  return dir;
}
