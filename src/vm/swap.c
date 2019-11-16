#include "vm/swap.h"
#include "devices/block.h"
#include "lib/kernel/bitmap.h"
#include "threads/vaddr.h"

#define SECTOR_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

struct block * swap_disk;
struct bitmap * swap_table;

void
swap_init(void){
    swap_disk = block_get_role(BLOCK_SWAP);
    swap_table = bitmap_create(SECTOR_PER_PAGE);
    bitmap_set_all(swap_table, true);
}

/*
victim page가 발생하여 victim page의 frame에 닮겨있던 정보를 swap_table로 swap_out함
아마 frame_allocate에서 불릴거같음
return saved index of bitmap 
*/
size_t
swap_out(void* physical_memory){
    size_t saved_index = bitmap_scan_and_flip(swap_table, 0, 1, true);
    if (saved_index==NULL){
        print("SWAP DISK full!!\n\n");
    }
    
    block_sector_t sector;
    void * memory_partion; 
        
    sector = BLOCK_SECTOR_SIZE * saved_index;
    for(int i=0; i<SECTOR_PER_PAGE; i++){
        memory_partion = physical_memory + BLOCK_SECTOR_SIZE * i;
        block_write(swap_table, saved_index*SECTOR_PER_PAGE+i, sector);
        sector ++;
    }
    return saved_index;
}

void
swap_in(void){

}
