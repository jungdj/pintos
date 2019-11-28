#include <bitmap.h>
#include "vm/swap.h"
#include "devices/block.h"
#include "threads/synch.h"
#include "threads/vaddr.h"


/* The swap device */
static struct block *swap_block;

/* Tracks in-use and free swap slots */
static struct bitmap *swap_table;

/* Protects swap_table */
static struct lock swap_lock;

static const size_t sectors_per_page = (PGSIZE / BLOCK_SECTOR_SIZE);

/* 
 * Initialize swap_block, swap_table, and swap_lock.
 */
void 
swap_init (void)
{
  swap_block = block_get_role (BLOCK_SWAP);
  lock_init (&swap_lock);
  swap_table = bitmap_create (block_size (swap_block) / sectors_per_page);
}

/*
 * Reclaim a frame from swap device.
 * 1. Check that the page has been already evicted. 
 * 2. You will want to evict an already existing frame
 * to make space to read from the disk to cache. 
 * 3. Re-link the new frame with the corresponding supplementary
 * page table entry. 
 * 4. Do NOT create a new supplementray page table entry. Use the 
 * already existing one. 
 * 5. Use helper function read_from_disk in order to read the contents
 * of the disk into the frame. 
 */ 
bool 
swap_in (size_t swap_index, void *page)
{
  lock_acquire(&swap_lock);
  ASSERT (bitmap_test(swap_table, swap_index));

  size_t i;
  for (i = 0; i < sectors_per_page; ++i) {
    block_read (swap_block, swap_index * sectors_per_page + i, page + (BLOCK_SECTOR_SIZE * i));
  }

  bitmap_set(swap_table, swap_index, false);
  lock_release(&swap_lock);
}

/* 
 * Evict a frame to swap device. 
 * 1. Choose the frame you want to evict. 
 * (Ex. Least Recently Used policy -> Compare the timestamps when each 
 * frame is last accessed)
 * 2. Evict the frame. Unlink the frame from the supplementray page table entry
 * Remove the frame from the frame table after freeing the frame with
 * pagedir_clear_page. 
 * 3. Do NOT delete the supplementary page table entry. The process
 * should have the illusion that they still have the page allocated to
 * them. 
 * 4. Find a free block to write you data. Use swap table to get track
 * of in-use and free swap slots.
 */
size_t
swap_out (void *addr)
{
  lock_acquire(&swap_lock);
  // TODO: Validation?
  size_t swap_index;
  size_t i;
  swap_index = bitmap_scan (swap_table, 0, 1, false);
  for (i = 0; i < sectors_per_page; ++i) {
    block_write (swap_block, swap_index * sectors_per_page + i, addr + (BLOCK_SECTOR_SIZE * i));
  }
  bitmap_set (swap_table, swap_index, true);
  lock_release(&swap_lock);
  return swap_index;
}


void
free_swap_slot (size_t swap_index)
{
  lock_acquire(&swap_lock);
  bitmap_set (swap_table, swap_index, true);
  lock_release(&swap_lock);
}


/* 
 * Read data from swap device to frame. 
 * Look at device/disk.c
 */
void read_from_disk (uint8_t *frame, int index)
{


}

/* Write data to swap device from frame */
void write_to_disk (uint8_t *frame, int index)
{


}

