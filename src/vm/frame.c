#include "lib/kernel/hash.h"
#include "threads/pte.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "userprog/pagedir.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "frame.h"
#include <stdio.h>
#include <list.h>
#include <stdint.h>

static struct hash frame_hash;
static struct lock frame_lock;
static struct hash_iterator * evict_iterator;

unsigned frame_hash_func(const struct hash_elem *e, void *aux);
bool frame_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux);
struct frame_entry * iter_to_frame_entry(struct hash_iterator *i);
bool iter_check_accessed(struct hash_iterator *i);

void
frame_init(){
    hash_init(&frame_hash, frame_hash_func, frame_less_func, NULL);
    lock_init(&frame_lock);
}

/*
process.c에 원래 있던 palloc_get_page를 대체하는 함수
parameter랑 return을 비슷하게 만들면 될 듯?
*/
void *
allocate_new_frame(enum palloc_flags flag, void * upage){
    void * new_page = palloc_get_page(PAL_USER | flag); /*frame must "PAL_USER"*/
    //printf("what is new frame for it ? : %p\n", new_page);
    
    /*eviction mode*/
    if (new_page==NULL){
        /*init iterator or after one cycle*/
        if (evict_iterator == NULL){
            hash_first(evict_iterator, &frame_hash);
        }
        
        // unsigned frame_num = (unsigned) hash_size(&frame_hash);
        // unsigned iter_num = 0;
    
        struct frame_entry * iter_frame_entry;
        /*no need to check protected in while loop. becasue it must be accessed*/
        while(iter_check_accessed(evict_iterator)){
            iter_frame_entry = iter_to_frame_entry(evict_iterator);
            
            /*보호 받는 중일때 통과. 아니면 변경*/
            if (!iter_frame_entry->protected){
                pagedir_set_accessed(iter_frame_entry->t->pagedir, iter_frame_entry->physical_memory, false);
            }
            evict_iterator = hash_next(evict_iterator);
        }

        /*죽일 애가 담겨있어서 죽이는 과정이 필요함
        swap out 시키고
        supplementary page에 기록하고
        원래 있던 프레임 entry를 free시킨다.
        */
        iter_frame_entry = iter_to_frame_entry(evict_iterator);
        size_t swap_table_idx = swap_out(iter_frame_entry->physical_memory);
        if (swap_table_idx == NULL){
            printf("PANIC!! swap is full");
        }

        /*update sup page table entry*/
        struct sup_pagetable_entry * sup_entry = sup_lookup(iter_frame_entry->allocated_page);
        sup_entry->status = SWAPPED;
        sup_entry->physical_memory = NULL;
        sup_entry->swap_table_idx = swap_table_idx;
        
        /*free frame entry*/
        free(iter_frame_entry);
    }
    /* if no new_page?? --> evcition*/
    struct frame_entry * frame_entry = malloc(sizeof(struct frame_entry));
    /* if malloc failed?? */
    frame_entry->t = thread_current();
    frame_entry->allocated_page = upage;
    frame_entry->physical_memory = new_page;
    frame_entry->protected = false;

    pagedir_set_accessed(frame_entry->t->pagedir, upage, true);

    /* lock before modifying hash */
    //printf("hash size ? : %d\n", frame_hash.elem_cnt);
    lock_acquire(&frame_lock);
    hash_insert(&frame_hash, &frame_entry->elem);
    lock_release(&frame_lock);
    //printf("hash size ? : %d\n", frame_hash.elem_cnt);
    return (void *)new_page; 
}


/*
마찬가지로 proceess.c 에 있는 palloc_free_page와 유사하게 구현
page를 free하고 frame table에서도 제거
*/
void
deallocate_frame(void * kpage){
    /*temp frame entry to find real frame_entry*/
    struct frame_entry * temp_frame_entry = (struct frame_entry *)malloc(sizeof(struct frame_entry));
    /* if malloc failed?? */
    temp_frame_entry->physical_memory = kpage;
    printf("here?\n\n");
    struct hash_elem * existed_elem = hash_find(&frame_hash, &temp_frame_entry->elem);
    free(temp_frame_entry);
    if (existed_elem == NULL){
        return false;
    }

    struct frame_entry * existed_frame_entry = hash_entry(existed_elem, struct frame_entry, elem);
    palloc_free_page(kpage);
    
    lock_acquire(&frame_lock);
    hash_delete(&frame_hash, &existed_frame_entry->elem);
    lock_release(&frame_lock);

    free(existed_frame_entry);
}

struct frame_entry *
lookup_frame(void * ppage){
    struct frame_entry * temp_frame_entry = (struct frame_entry *)malloc(sizeof(struct frame_entry));
    temp_frame_entry->physical_memory = ppage;
    printf("here?\n\n");
    //printf("physical memory : %p\n", ppage);
    struct hash_elem * find_elem = hash_find(&frame_hash, &temp_frame_entry->elem);
    
    if (find_elem == NULL){
        //printf("Can not find matched elem!\n\n");
        free(temp_frame_entry);
        return NULL;
    }
    //printf("Find matched elem!\n\n");
    struct frame_entry * find_entry = hash_entry(find_elem, struct frame_entry, elem);    
    free(temp_frame_entry);

    return find_entry;
}

/*
frame_hash_function by tid로 하려고 했으나... deallocate에서 구현 문제 발생
deallocate하려면 page만 가지고 어떤 hash elem일지 유추 가능해야함... list로 짤까?
physical memory로 hash시킴
*/
unsigned
frame_hash_func (const struct hash_elem *e, void *aux UNUSED){
    struct frame_entry *frame_entry = hash_entry (e, struct frame_entry, elem);
    return hash_bytes(frame_entry->physical_memory, sizeof(frame_entry->physical_memory));
    //return hash_int((int)&frame_entry->physical_memory);
}

bool
frame_less_func (const struct hash_elem *a,
                 const struct hash_elem *b,
                 void *aux UNUSED){
    struct frame_entry *a_entry = hash_entry(a, struct frame_entry, elem);
    struct frame_entry *b_entry = hash_entry(b, struct frame_entry, elem);
    return a_entry->physical_memory < b_entry->physical_memory;
}

struct frame_entry *
iter_to_frame_entry(struct hash_iterator *i){
    return hash_entry(hash_cur(i), struct frame_entry, elem);
}

/*if accessed = true, else false*/
bool
iter_check_accessed(struct hash_iterator *i){
    struct frame_entry * iter_entry = iter_to_frame_entry(i);
    return pagedir_is_accessed(iter_entry->t->pagedir, iter_entry->physical_memory);
}