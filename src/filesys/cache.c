#include "filesys/cache.h"

#include "devices/block.h"
#include "threads/synch.h"
#include "filesys/filesys.h"

#define CACHE_CNT 64

static struct cache_entry cache_array[CACHE_CNT];

void buffer_cache_init(void){
    for(int i=0; i<CACHE_CNT; i++){
        lock_init(cache_array[i].cache_lock);
    }
}

//Prepare for filesys_done
void buffer_cache_close(void){
    /*Todo flush*/
    return;
}

//Substitute of block_write
void buffer_cache_write(block_sector_t sector, const void *buffer){
    /*Todo write*/
    block_write(fs_device, sector, buffer);
};

//Substitute of block_read
void buffer_cache_read(block_sector_t sector, void * buffer){
    block_read(fs_device, sector, buffer);
};
