#include "devices/block.h"
#include "threads/synch.h"

#define CACHE_CNT 64

struct cache_entry{
    bool is_use; 

    // struct lock * cache_lock; /*Individual lock for entry*/
    block_sector_t sector; /* data_idx */
    char data[BLOCK_SECTOR_SIZE]; /*Real block data*/

    bool is_dirty;
    bool is_accessed;
};

//Substitute of block_read, block_write
void buffer_cache_init(void);
void buffer_cache_close(void);
void buffer_cache_write(block_sector_t sector, const void *buffer);
void buffer_cache_read(block_sector_t sector, void * buffer);
