#include "vm/swap.h"
#include "devices/block.h"
#include "lib/kernel/bitmap.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

#define SECTOR_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

static struct block * swap_disk;
static struct bitmap * swap_table;
static struct lock swap_lock;
static size_t swap_size;

void
swap_init(void){
    lock_init(&swap_lock);
    swap_disk = block_get_role(BLOCK_SWAP);
    if(swap_disk == NULL){
        PANIC ("Error: Can't initialize swap block");
    }
    swap_size = block_size(swap_disk) / SECTOR_PER_PAGE;
    swap_table = bitmap_create(swap_size);
    bitmap_set_all(swap_table, true);
}

/*
victim page가 발생하여 victim page의 frame에 닮겨있던 정보를 swap_table로 swap_out함
아마 frame_allocate에서 불릴거같음
return saved index of bitmap 
*/
size_t
swap_out(void* physical_memory){
    //printf("swap out start\n");
    
    // printf("bitmap_scan true : %d\n", bitmap_scan(swap_disk, 0, 1, true));
    // printf("bitmap_scan false : %d\n", bitmap_scan(swap_disk, 0, 1, false));
    lock_acquire(&swap_lock);
    size_t saved_index = bitmap_scan_and_flip(swap_table, 0, 1, true);
    lock_release(&swap_lock);
    
    if (saved_index==-1){
        printf("SWAP DISK full!!\n\n");
    }
    
    block_sector_t sector;
    void * memory_partion; 

    sector = physical_memory + BLOCK_SECTOR_SIZE/* * saved_index*/;
    for(int i=0; i<SECTOR_PER_PAGE; i++){
        memory_partion = physical_memory + BLOCK_SECTOR_SIZE * i;
        block_write(swap_disk, saved_index*SECTOR_PER_PAGE+i, sector);
        sector += BLOCK_SECTOR_SIZE;
    }
    // printf("swap out finish\n");
    return saved_index;
}

/*data swapping_in */
void
swap_in(size_t idx, void * physical_memory){
    printf("swap_in start!\n");
    /* swap 데이터 새로 발급받은 physical memory에 복사*/
    block_sector_t sector;
    void * memory_partion;

    sector = BLOCK_SECTOR_SIZE * idx;
    for (int i =0; i<SECTOR_PER_PAGE; i++){
        memory_partion = physical_memory + BLOCK_SECTOR_SIZE * i;
        block_read(swap_disk, sector, memory_partion);
        sector ++;
    }
    
    /*bitmap set 변경*/
    lock_acquire(&swap_lock);
    bitmap_set(swap_table, idx, true);
    lock_release(&swap_lock);
}


/*call in hash_destroy in page*/
void
swap_free(size_t idx){
    /* 해당 index의 disk 날리고 ==> 꼭 날려야 하나? 참조하는 값만 없애는게 더 효율적일듯
    해당 index 값 true로 변경하면 끝인듯?*/
    lock_acquire(&swap_lock);
    bitmap_set(swap_table, idx, true);
    lock_release(&swap_lock);
}