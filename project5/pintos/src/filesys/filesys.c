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
  /* For Persistence */
  free_map_close ();
  buffer_cache_terminate (); // buffer cache flush all
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size, bool is_dir) 
{
  block_sector_t inode_sector = 0;
//  struct dir *dir = dir_open_root (); // create file in root
  /* "name" parsing하여 directory path와 생성할 file(directory) name 구분 */
  char *directory = (char *)malloc(sizeof(char) * (strlen(name) + 1));
  char *file = (char *)malloc(sizeof(char) * (NAME_MAX + 1));
  parse_path (name, directory, file);	// "/" 기준으로 parsing
  struct dir *dir = open_directory_path (directory); // parsing한 subdirectory open 

  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, is_dir)
                  && dir_add (dir, /*name*/file, inode_sector));
	
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
  char *directory = (char *)malloc(sizeof(char) * (strlen(name) + 1));
  char *file = (char *)malloc(sizeof(char) * (NAME_MAX + 1));

  if(!strcmp(name, "/")){
    return file_open (inode_open (ROOT_DIR_SECTOR)); 
  }

  parse_path (name, directory, file);
//  struct dir *dir = dir_open_root ();
  struct dir *dir = open_directory_path (directory);
  struct inode *inode = NULL;


  if (dir != NULL) 
    dir_lookup (dir, file, &inode);
  dir_close (dir);
/*
  if (dir != NULL) {
    dir_lookup (dir, file, &inode);
    dir_close (dir);
  }
  else
    inode = dir_get_inode (dir);
  if(inode == NULL || inode_is_remove (inode))
    return NULL;
*/
  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  char *directory = (char *)malloc(sizeof(char) * (strlen(name) + 1));
  char *file = (char *)malloc(sizeof(char) * (NAME_MAX + 1));
  parse_path (name, directory, file);
//  struct dir *dir = dir_open_root ();
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

void parse_path (char *name, char *directory, char *file)
{
  char str[NAME_MAX + 1];
  strlcpy(str, name, strlen(name) + 1);
  char *next_ptr;
  char *ptr = strtok_r(str, "/", &next_ptr);
  
  if(name[0] == '/')
    strlcpy(directory, "/", 2);
  else
    directory[0] = '\0';
  file[0] = '\0';
  while(ptr){
    if(strlen(file)){
      strlcat(directory, file, strlen(directory) + strlen(file) + 1);
      strlcat(directory, "/", strlen(directory) + 2);
    }

    strlcpy (file, ptr, strlen(ptr) + 1);
    ptr = strtok_r(NULL, "/", &next_ptr);
  }
}
