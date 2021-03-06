#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "filesys/cache.h"

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
  
  buffer_cache_init();
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
  buffer_cache_close();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size, bool is_dir) 
{
  block_sector_t inode_sector = 0;
  
  char dir_path[strlen(name)+1], file_name[strlen(name)+1];
  memset (dir_path, 0, sizeof(dir_path));
  memset (file_name, 0, sizeof(file_name));
  split_path(name, dir_path, file_name);
//  printf ("dir_path : %s, file_name : %s\n", dir_path, file_name);
  struct dir *dir = dir_open_path(dir_path);

  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, is_dir)
                  && dir_add (dir, file_name, inode_sector, is_dir));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  dir_close (dir);

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
  char dir_path[strlen(name)+1], file_name[strlen(name)+1];
  memset (dir_path, 0, sizeof(dir_path));
  memset (file_name, 0, sizeof(file_name));
  split_path(name, dir_path, file_name);
  struct dir *dir = dir_open_path(dir_path);

  struct inode *inode = NULL;

  if (dir != NULL) {
    if (strlen(dir_path) && !strlen(file_name))
      inode = inode_open (ROOT_DIR_SECTOR);
    else
      dir_lookup (dir, file_name, &inode);
  }
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
  char dir_path[strlen(name)+1], file_name[strlen(name)+1];
  memset (dir_path, 0, sizeof(dir_path));
  memset (file_name, 0, sizeof(file_name));
  split_path(name, dir_path, file_name);
  struct dir *dir = dir_open_path(dir_path);

  bool success = dir != NULL && dir_remove (dir, file_name);
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
