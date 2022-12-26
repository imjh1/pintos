#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

  if (format) 
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
  /* buffer cache flush all for persistence */
  buffer_cache_terminate (); 
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size, bool is_dir) 
{
  block_sector_t inode_sector = 0;
  /* 기존 root에서 파일 create하는 방식
  -> subdirectory를 parsing하여, 해당 directory의 파일을 create하는 방식으로 변경 */
  char *directory = (char *)malloc(sizeof(char) * (strlen(name) + 1));
  char *file = (char *)malloc(sizeof(char) * (NAME_MAX + 1));
  parse_path (name, directory, file);	// "name" parsing하여 directory path와 생성할 file(directory) name 구분 
  struct dir *dir = open_directory_path (directory); // parsing한 subdirectory open 

  /* directory에 entry추가하여 inode_sector에 저장 */  
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, is_dir)
                  && dir_add (dir, file, inode_sector));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  dir_close (dir);

  free(directory);
  free(file);
  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  /* "/"이 인자로 넘어온 예외 case 처리 */
  if(!strcmp(name, "/"))
    return file_open (inode_open (ROOT_DIR_SECTOR));

  /* 기존 root에서 파일 open하는 방식
  -> subdirectory를 parsing하여, 해당 directory의 파일을 open하는 방식으로 변경 */
  char *directory = (char *)malloc(sizeof(char) * (strlen(name) + 1));
  char *file = (char *)malloc(sizeof(char) * (NAME_MAX + 1));
  parse_path (name, directory, file);
  struct dir *dir = open_directory_path (directory);
  struct inode *inode = NULL;


  if (dir != NULL) 
    dir_lookup (dir, file, &inode);
  dir_close (dir);

  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  /* 기존 root에서 파일 remove하는 방식
  -> subdirectory를 parsing하여, 해당 directory의 파일을 remove하는 방식으로 변경 */
  char *directory = (char *)malloc(sizeof(char) * (strlen(name) + 1));
  char *file = (char *)malloc(sizeof(char) * (NAME_MAX + 1));
  parse_path (name, directory, file);
  struct dir *dir = open_directory_path (directory);
  
  bool success = dir != NULL && dir_remove (dir, file);
  dir_close (dir); 

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}

