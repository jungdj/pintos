#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include <stdio.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "filesys/cache.h"
#include "threads/malloc.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
#define DIRECT_BLOCK_CNT 123
#define INDIRECT_BLOCK_CNT 128

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    block_sector_t direct[DIRECT_BLOCK_CNT];/* data sectors. */
    block_sector_t indirect;                /* indirect sectors. */
    block_sector_t doubley_indirect;        /* doubly_indirect sectors. */
    off_t length;                       /* File size in bytes. */
    bool is_dir;                        /* Check whether inode is dir or not*/
    unsigned magic;                     /* Magic number. */
    // uint32_t unused[125];
  };

struct inode_for_indirect
  {
    block_sector_t indirect[INDIRECT_BLOCK_CNT];
  };

enum
sector_status
{
  DIRECT,
  INDIRECT,
  DOUBLEY_INDIRECT,
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

bool
inode_is_dir (struct inode *inode)
{
  return inode->data.is_dir;
}

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  struct inode_disk in_disk = inode->data;
  if (pos < in_disk.length) {
    off_t sector_idx = pos / BLOCK_SECTOR_SIZE;
    
    //status DIRECT case
    if(sector_idx<DIRECT_BLOCK_CNT){
      return in_disk.direct[sector_idx];
    
    //status INDIRECT case
    }else if(sector_idx<DIRECT_BLOCK_CNT + INDIRECT_BLOCK_CNT){
      block_sector_t ret;
      struct inode_for_indirect *indirect_inode;
      off_t idx_in_indirect = sector_idx - DIRECT_BLOCK_CNT;

      indirect_inode = calloc(1, sizeof(struct inode_for_indirect));
      buffer_cache_read(in_disk.indirect, indirect_inode, 0, BLOCK_SECTOR_SIZE);
      ret = indirect_inode->indirect[idx_in_indirect];
      
      free(indirect_inode);
      
      return ret;
    //status DOUBLEY_INDIRECT case
    }else{
      block_sector_t ret;
      struct inode_for_indirect *doubly_indirect_inode;
      struct inode_for_indirect *indirect_for_doubly;  
      off_t idx_in_doubly = (sector_idx-DIRECT_BLOCK_CNT-INDIRECT_BLOCK_CNT) / INDIRECT_BLOCK_CNT;
      off_t idx_in_indirect_for_doubly = (sector_idx-DIRECT_BLOCK_CNT-INDIRECT_BLOCK_CNT) % INDIRECT_BLOCK_CNT;

      doubly_indirect_inode = calloc(1, sizeof(struct inode_for_indirect));
      buffer_cache_read(in_disk.doubley_indirect, doubly_indirect_inode, 0, BLOCK_SECTOR_SIZE);
      indirect_for_doubly = calloc(1, sizeof(struct inode_for_indirect));
      buffer_cache_read(doubly_indirect_inode->indirect[idx_in_doubly], indirect_for_doubly, 0, BLOCK_SECTOR_SIZE);

      ret = indirect_for_doubly -> indirect[idx_in_indirect_for_doubly];

      free(doubly_indirect_inode);
      free(indirect_for_doubly);
      return ret;
    }
  }
  else
    return -1;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
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
  enum sector_status status = DIRECT;
  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);
  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      size_t sectors = bytes_to_sectors (length);
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      disk_inode->is_dir = is_dir;

      block_sector_t allocated_sectors_cnt = 0;
      
      //block sectors to compute
      block_sector_t *allocating_sector; // next sector to write
      uint32_t idx_in_indirect = 0; //
      uint32_t idx_in_doubly = 0; // when idx_in_indirect_for_doubly run INDIRECT_BLOCK_CNT times, than increase
      uint32_t idx_in_indirect_for_doubly = 0;

      // save pointers for use (free) at the end of this function
      // TODO. I don't consider calloc failed case.
      struct inode_for_indirect *indirect_inode= NULL;
      struct inode_for_indirect *doubly_indirect_inode = NULL;
      struct inode_for_indirect *indirect_for_doubly[INDIRECT_BLOCK_CNT] = {NULL};     
      
      //TODO filesys가 꽉차서 free_map_allocation이 실패하는 경우(BITMAP_ERROR return) 고려 X
      while (allocated_sectors_cnt != sectors){
        //direct case
        switch(status){
          case DIRECT:
            allocating_sector = &disk_inode->direct[allocated_sectors_cnt];
            break;
          
          case INDIRECT:
            idx_in_indirect = allocated_sectors_cnt-DIRECT_BLOCK_CNT;
            //allocate new inode indirect inode
            if(!disk_inode->indirect){
              free_map_allocate(1, &disk_inode->indirect);
              indirect_inode = (struct inode_for_indirect *)calloc(1, sizeof(struct inode_for_indirect));
            }
            allocating_sector = &indirect_inode->indirect[idx_in_indirect];
            break;
          
          case DOUBLEY_INDIRECT:
            idx_in_doubly = (allocated_sectors_cnt-DIRECT_BLOCK_CNT-INDIRECT_BLOCK_CNT) / INDIRECT_BLOCK_CNT;
            idx_in_indirect_for_doubly = (allocated_sectors_cnt-DIRECT_BLOCK_CNT-INDIRECT_BLOCK_CNT) % INDIRECT_BLOCK_CNT;
            
            //allocate new doubly indirect inode. Just one time.
            if(!disk_inode->doubley_indirect){
              free_map_allocate(1, &disk_inode->doubley_indirect);
              doubly_indirect_inode = (struct inode_for_indirect *)calloc(1, sizeof(struct inode_for_indirect));
            }

            //allocate new indirect inode for doubly connected.
            if(!doubly_indirect_inode->indirect[idx_in_doubly]){
              free_map_allocate(1, &doubly_indirect_inode->indirect[idx_in_doubly]);
              indirect_for_doubly[idx_in_doubly] = (struct inode_for_indirect *)calloc(1, sizeof(struct inode_for_indirect));
            }
            allocating_sector = &indirect_for_doubly[idx_in_doubly]->indirect[idx_in_indirect_for_doubly];
            break;
          
          default:
            PANIC("PANIC!! Cannot reach here");
            break;
        }
        //It seems success. then map disk_node to 
        free_map_allocate(1, allocating_sector);

        switch (status) {
          case DIRECT:
            buffer_cache_write(sector, disk_inode, 0, BLOCK_SECTOR_SIZE);
            break;
          case INDIRECT:
            if (!idx_in_indirect) {
              buffer_cache_write(sector, disk_inode, 0, BLOCK_SECTOR_SIZE);
            }
            buffer_cache_write(disk_inode->indirect, indirect_inode, 0, BLOCK_SECTOR_SIZE);
            break;
          case DOUBLEY_INDIRECT:
            if (!idx_in_doubly) {
              buffer_cache_write(sector, disk_inode, 0, BLOCK_SECTOR_SIZE);
            }
            if (!idx_in_indirect_for_doubly) {
              buffer_cache_write(disk_inode->doubley_indirect, doubly_indirect_inode, 0, BLOCK_SECTOR_SIZE);
            }
            buffer_cache_write (doubly_indirect_inode->indirect[idx_in_doubly], indirect_for_doubly[idx_in_doubly], 0, BLOCK_SECTOR_SIZE);
            break;
          default:
            PANIC("Cannot reach here!");
            break;
        }

        // buffer_cache_write (allocating_sector, zeros, 0, BLOCK_SECTOR_SIZE);

        //go to next sectors
        allocated_sectors_cnt ++;

        //Change status
        switch (status)
        {
        case DIRECT:
          if (allocated_sectors_cnt == DIRECT_BLOCK_CNT)
            status = INDIRECT;
          break;
        case INDIRECT:
          if (allocated_sectors_cnt == DIRECT_BLOCK_CNT+INDIRECT_BLOCK_CNT)
            status = DOUBLEY_INDIRECT;
          break;
        default:
          /*do nothing*/
          break;
        }
      }
      
      success = true;
      buffer_cache_write(sector, disk_inode, 0, BLOCK_SECTOR_SIZE);

      free (disk_inode);
      free (indirect_inode);
      free (doubly_indirect_inode);
      for(int i=0; i<INDIRECT_BLOCK_CNT; i++){
        free (indirect_for_doubly[i]);
      }
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
  buffer_cache_read (inode->sector, &inode->data, 0, BLOCK_SECTOR_SIZE);
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
      //TODO. dir case. Delete after check dir is empty 
      if (inode->removed) 
        {
          enum sector_status status = DIRECT;
          struct inode_disk * in_disk = &inode->data;
          size_t sector_cnt = in_disk->length / BLOCK_SECTOR_SIZE;
          size_t removed_cnt = 0;

          block_sector_t idx_in_indirect;
          block_sector_t idx_in_doubly;
          block_sector_t idx_in_indirect_for_doubly;

          struct inode_for_indirect *indirect_inode = NULL;
          struct inode_for_indirect *doubly_indirect_inode = NULL;
          struct inode_for_indirect *indirect_for_doubly[INDIRECT_BLOCK_CNT] = {NULL};

          while(sector_cnt != removed_cnt){
            switch (status)
            {
              case DIRECT:
                free_map_release(in_disk->direct[removed_cnt], 1);
                break;
              case INDIRECT:
                idx_in_indirect = removed_cnt-DIRECT_BLOCK_CNT;
                if(idx_in_indirect == 0){
                  indirect_inode = (struct inode_for_indirect *)calloc(1, sizeof(struct inode_for_indirect));
                  buffer_cache_read(in_disk->indirect, indirect_inode, 0, BLOCK_SECTOR_SIZE);
                  free_map_release(in_disk->indirect, 1);
                }
                ASSERT(indirect_inode != NULL);
                free_map_release(indirect_inode->indirect[idx_in_indirect], 1);
                break;
              case DOUBLEY_INDIRECT:
                idx_in_doubly = (removed_cnt-DIRECT_BLOCK_CNT-INDIRECT_BLOCK_CNT)/BLOCK_SECTOR_SIZE;
                idx_in_indirect_for_doubly = (removed_cnt-DIRECT_BLOCK_CNT-INDIRECT_BLOCK_CNT)&BLOCK_SECTOR_SIZE;

                if(idx_in_doubly == 0 && idx_in_indirect_for_doubly == 0){
                  doubly_indirect_inode = (struct inode_for_indirect *)calloc(1, sizeof(struct inode_for_indirect));
                  buffer_cache_read(in_disk->doubley_indirect, doubly_indirect_inode, 0, BLOCK_SECTOR_SIZE);
                  free_map_release(in_disk->doubley_indirect, 1);
                }
                if(idx_in_indirect_for_doubly == 0){
                  ASSERT(doubly_indirect_inode != NULL);
                  indirect_for_doubly[idx_in_doubly] = (struct inode_for_indirect *)calloc(1, sizeof (struct inode_for_indirect));
                  buffer_cache_read(doubly_indirect_inode->indirect[idx_in_doubly], indirect_for_doubly[idx_in_doubly], 0, BLOCK_SECTOR_SIZE);
                  free_map_release(doubly_indirect_inode->indirect[idx_in_doubly], 1);
                }
                ASSERT(indirect_for_doubly != NULL);
                free_map_release(indirect_for_doubly[idx_in_doubly]->indirect[idx_in_indirect_for_doubly], 1);
                break;
              default:
                PANIC("PANIC!! Inode_close. Cannot reach here");
                break;
            }

            removed_cnt ++;

            //Change status
            switch (status)
            {
            case DIRECT:
              if (removed_cnt == DIRECT_BLOCK_CNT)
                status = INDIRECT;
              break;
            case INDIRECT:
              if (removed_cnt == DIRECT_BLOCK_CNT+INDIRECT_BLOCK_CNT)
                status = DOUBLEY_INDIRECT;
              break;
            default:
              /*do nothing*/
              break;
            }
          }

          free (indirect_inode);
          free (doubly_indirect_inode);
          for(int i=0; i<INDIRECT_BLOCK_CNT; i++){
            free (indirect_for_doubly[i]);
          }
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

      if (sector_idx == (block_sector_t) -2) {
        memset (buffer+bytes_read, 0, chunk_size);
      } else {
        buffer_cache_read(sector_idx, buffer + bytes_read, sector_ofs, chunk_size);
      }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }

  return bytes_read;
}

void
inode_append_sector (struct inode *inode, block_sector_t sector_idx, off_t size)
{
  int sector_cnt = bytes_to_sectors (inode_length (inode));
  struct inode_disk *disk_inode = &inode->data;
  struct inode_for_indirect *indirect_inode = NULL;
  struct inode_for_indirect *doubly_indirect_inode = NULL;
  struct inode_for_indirect *indirect_for_doubly[INDIRECT_BLOCK_CNT] = {NULL};

  disk_inode->length = sector_cnt * BLOCK_SECTOR_SIZE + size;

  if (sector_cnt < DIRECT_BLOCK_CNT) {
    disk_inode->direct[sector_cnt] = sector_idx;
  } else if (sector_cnt < DIRECT_BLOCK_CNT + INDIRECT_BLOCK_CNT) {
    indirect_inode = (struct inode_for_indirect *)calloc(1, sizeof(struct inode_for_indirect));
    buffer_cache_read (disk_inode->indirect, indirect_inode, 0, BLOCK_SECTOR_SIZE);
    indirect_inode->indirect[sector_cnt - DIRECT_BLOCK_CNT] = sector_idx;
    buffer_cache_write (disk_inode->indirect, indirect_inode, 0, BLOCK_SECTOR_SIZE);
    free (indirect_inode);
  } else {
    /* Doubly Indirect */
    doubly_indirect_inode = (struct inode_for_indirect *)calloc(1, sizeof(struct inode_for_indirect));
    buffer_cache_read (disk_inode->doubley_indirect, doubly_indirect_inode, 0, BLOCK_SECTOR_SIZE);
    int idx_in_doubly = (sector_cnt - DIRECT_BLOCK_CNT - INDIRECT_BLOCK_CNT) / INDIRECT_BLOCK_CNT;
    int idx_in_indirect_for_doubly = (sector_cnt - DIRECT_BLOCK_CNT - INDIRECT_BLOCK_CNT) % INDIRECT_BLOCK_CNT;

    if (!idx_in_indirect_for_doubly) {
      block_sector_t indirect_for_doubly_idx;
      free_map_allocate (1, &indirect_for_doubly_idx);
      doubly_indirect_inode->indirect[idx_in_doubly] = indirect_for_doubly_idx;
      buffer_cache_write (disk_inode->doubley_indirect, doubly_indirect_inode, 0, BLOCK_SECTOR_SIZE);
    }

    indirect_for_doubly[idx_in_doubly] = (struct inode_for_indirect *)calloc(1, sizeof(struct inode_for_indirect));
    buffer_cache_read (doubly_indirect_inode->indirect[idx_in_doubly], indirect_for_doubly[idx_in_doubly], 0, BLOCK_SECTOR_SIZE);
    indirect_for_doubly[idx_in_doubly]->indirect[idx_in_indirect_for_doubly] = sector_idx;
    buffer_cache_write (doubly_indirect_inode->indirect[idx_in_doubly], indirect_for_doubly[idx_in_doubly], 0, BLOCK_SECTOR_SIZE);

    free (doubly_indirect_inode);
    free (indirect_for_doubly[idx_in_doubly]);
  }
  buffer_cache_write (inode->sector, disk_inode, 0, BLOCK_SECTOR_SIZE);
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

  if (inode->deny_write_cnt)
    return 0;

  block_sector_t sector_idx = byte_to_sector (inode, offset);
  if (sector_idx == (block_sector_t) -1 && size>0){
    /* Offset out of inode data */
    off_t offset_from_inode_data = offset - bytes_to_sectors (inode_length (inode)) * BLOCK_SECTOR_SIZE;

    if (offset_from_inode_data < 0) {
      inode->data.length = offset+1;
      buffer_cache_write (inode->sector, &inode->data, 0, BLOCK_SECTOR_SIZE);
    }

    while (offset_from_inode_data >= BLOCK_SECTOR_SIZE) {
      inode_append_sector (inode, (block_sector_t) -2, BLOCK_SECTOR_SIZE); // inode_length has been updated
      offset_from_inode_data = offset - inode_length (inode);
    }

    // The last block, containing offset
    if (offset_from_inode_data > 0) {
      inode_append_sector (inode, (block_sector_t) -2, offset_from_inode_data + 1);
    } // else : offset at edge of block
  }

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      bool new_idx = false;
      /* allocate new sector_idx*/
      if (sector_idx == (block_sector_t) -1){
        free_map_allocate(1, &sector_idx);
        new_idx = true;
      } else if (sector_idx == (block_sector_t) -2) {
        free_map_allocate(1, &sector_idx);
      }

      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < sector_left ? size : sector_left;
      
      // if (chunk_size <= 0)
      //   break;
      
      if (!new_idx && inode_left < BLOCK_SECTOR_SIZE && size > inode_left){
      /*Growth case - last block*/
        if(size+sector_ofs > BLOCK_SECTOR_SIZE){
          inode->data.length = offset + sector_left;
          //size가 커서 sector_left == chunk_size
          buffer_cache_write (inode->sector, &inode->data, 0, BLOCK_SECTOR_SIZE);
        }else{
          /*Just Length Growth*/
          inode->data.length = offset + chunk_size;
          buffer_cache_write (inode->sector, &inode->data, 0, BLOCK_SECTOR_SIZE);
        }
      /*Not last block*/
      }

      buffer_cache_write(sector_idx, buffer + bytes_written, sector_ofs, chunk_size);

      if(new_idx) {
        /*append sector*/
        inode_append_sector (inode, sector_idx, chunk_size);
      }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
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
