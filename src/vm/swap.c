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
        printf("SWAP DISK full!!\n\n");
    }
    
    block_sector_t sector;
    void * memory_partion; 
        
    sector = BLOCK_SECTOR_SIZE * saved_index;
    for(int i=0; i<SECTOR_PER_PAGE; i++){
        memory_partion = physical_memory + BLOCK_SECTOR_SIZE * i;
        block_write(swap_disk, saved_index*SECTOR_PER_PAGE+i, sector);
        sector ++;
    }
    return saved_index;
}

/*data swapping_in */
void
swap_in(size_t idx, void * physical_memory){
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
    bitmap_set(swap_table, idx, true);
}


/*call in hash_destroy in page*/
void
swap_free(size_t idx){
    /* 해당 index의 disk 날리고 ==> 꼭 날려야 하나? 참조하는 값만 없애는게 더 효율적일듯
    해당 index 값 true로 변경하면 끝인듯?*/
    bitmap_set(swap_table, idx, true);
}