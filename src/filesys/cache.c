#include "filesys/cache.h"

#include "lib/debug.h"
#include "lib/string.h"
#include "devices/block.h"
#include "threads/synch.h"
#include "filesys/filesys.h"

#define CACHE_CNT 64

static struct cache_entry cache_array[CACHE_CNT];
static int second_chance_idx;
static struct lock cache_lock;

struct cache_entry * buffer_cache_lookup(block_sector_t sector);
struct cache_entry * find_entry_to_store(void);
int get_idx(void);

void buffer_cache_init(void){
    for(int i=0; i<CACHE_CNT; i++){
    //     lock_init(cache_array[i].cache_lock);
        cache_array[i].is_use = false;
    }
    lock_init(&cache_lock);
    second_chance_idx = 0;
}

//Prepare for filesys_done, do flush
void buffer_cache_close(void){
    // lock_acquire(&cache_lock);
    for(int i=0; i<CACHE_CNT; i++){
        if(cache_array[i].is_use || cache_array[i].is_dirty){
            block_write(fs_device, cache_array[i].sector, cache_array[i].data);
            cache_array[i].is_dirty = false;
        }
    }
    // lock_release(&cache_lock);
    return;
}

//Substitute of block_write
//default option: sector_ofs = 0, chunk_size = BLOCK_SECTOR_SIZE
void buffer_cache_write(block_sector_t sector, const void *buffer, int sector_ofs, int chunk_size){
    struct cache_entry * entry;
    
    lock_acquire(&cache_lock);
    entry = buffer_cache_lookup(sector);
    if (entry == NULL){
        entry = find_entry_to_store();
        entry->is_use = true;
        entry->sector = sector;
    }
    /*Todo write*/
    memcpy(entry->data + sector_ofs, buffer, chunk_size);
    entry->is_accessed = true;
    entry->is_dirty=true;
    lock_release(&cache_lock);
};

//Substitute of block_read
void buffer_cache_read(block_sector_t sector, void * buffer, int sector_ofs, int chunk_size){
    struct cache_entry * entry;

    lock_acquire(&cache_lock);
    entry = buffer_cache_lookup(sector);
    //Need to read by block
    if (entry == NULL){
        entry = find_entry_to_store();
        entry->is_use = true;
        entry->sector = sector;
        entry->is_dirty = false;
        block_read(fs_device, sector, entry->data);
    }
    memcpy(buffer, entry->data + sector_ofs, chunk_size);
    entry->is_accessed = true;
    lock_release(&cache_lock);   
};

/* Find cache array element
*/
struct cache_entry * buffer_cache_lookup(block_sector_t sector){
    ASSERT( lock_held_by_current_thread(&cache_lock));
    for(int i=0; i<CACHE_CNT; i++){
        if(cache_array[i].is_use && cache_array[i].sector == sector){
            return &cache_array[i];        
        }
    }
    return NULL;
};

struct cache_entry * find_entry_to_store(void){
    ASSERT( lock_held_by_current_thread(&cache_lock));
    //Need not eviction case
    for(int i=0; i<CACHE_CNT; i++){
        if(!cache_array[i].is_use){
            return &cache_array[i];        
        }
    }

    int current_idx;
    //eviction case
    for(int i=0; i<CACHE_CNT*2; i++){
        current_idx = get_idx();
        if(cache_array[current_idx].is_accessed){
            cache_array[current_idx].is_accessed = false;
        }else{
            /*Todo Dirty bit check*/
            if(cache_array[current_idx].is_dirty){
                block_write(fs_device, cache_array[current_idx].sector, cache_array[current_idx].data);
            }
            return &cache_array[current_idx];
        }
    }
    printf("PANIC : Can not reach here!!\n");    
    return NULL;
};

int get_idx(void){
    int ret = second_chance_idx;
    second_chance_idx = (second_chance_idx+1)%CACHE_CNT;
    return ret;
}