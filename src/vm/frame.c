#include "lib/kernel/list.h"
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
#include "threads/malloc.h"

static struct hash frame_hash;
static struct lock frame_lock;
static struct list frame_list;
static struct list_elem * list_index;



unsigned frame_hash_func(const struct hash_elem *e, void *aux);
bool frame_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux);
// struct frame_entry * iter_to_frame_entry(struct hash_iterator *i);
// bool iter_check_accessed(struct hash_iterator *i);

void evict_frame(void);
void list_insert_before_index(struct list_elem * elem);
void list_index_move(void);

void
frame_init(){
    hash_init(&frame_hash, frame_hash_func, frame_less_func, NULL);
    lock_init(&frame_lock);
    list_init(&frame_list);
    list_index = NULL;
}

/*
process.c에 원래 있던 palloc_get_page를 대체하는 함수
parameter랑 return을 비슷하게 만들면 될 듯?
*/
void *
allocate_new_frame(enum palloc_flags flag, void * upage){
    //printf("start allocate\n");
    void * new_page = palloc_get_page(PAL_USER | flag); /*frame must "PAL_USER"*/
    if (new_page==NULL){
        evict_frame(); // make new page
        new_page = palloc_get_page(PAL_USER | flag);
    }
    ASSERT (new_page != NULL)
    struct frame_entry * frame_entry = malloc(sizeof(struct frame_entry));
    if (frame_entry == NULL){
        printf("PANIC malloc failed!!\n");
    }
    /* if malloc failed?? */
    frame_entry->t = thread_current();
    frame_entry->allocated_page = upage;
    frame_entry->physical_memory = new_page;
    frame_entry->protected = false;
    pagedir_set_accessed(frame_entry->t->pagedir, upage, true);
    
    /* lock before modifying hash */
    //printf("hash size ? : %d\n", frame_hash.elem_cnt);
    lock_acquire(&frame_lock);
    hash_insert(&frame_hash, &frame_entry->helem);
    list_insert_before_index(&frame_entry->lelem);
    lock_release(&frame_lock);

    //testing
    return (void *)new_page; 
}


/*
마찬가지로 proceess.c 에 있는 palloc_free_page와 유사하게 구현
page를 free하고 frame table에서도 제거
*/
void
deallocate_frame(void * kpage){
    struct frame_entry * existed_frame_entry = lookup_frame(kpage);    
    /*pop list, hash*/
    lock_acquire(&frame_lock);
    hash_delete(&frame_hash, &existed_frame_entry->helem);
    /* deallocated_frame에 list_index 가 위치해 있으면 위치 변경 */
    if(&existed_frame_entry->lelem == list_index){
        list_index_move();    
    }
    list_remove(&existed_frame_entry->lelem);
    lock_release(&frame_lock);

    /*free physical page*/
    //pagedir_clear_page(existed_frame_entry->t, existed_frame_entry->allocated_page);
    palloc_free_page(kpage);
    free(existed_frame_entry);
}

struct frame_entry *
lookup_frame(void * kpage){
    struct frame_entry * temp_frame_entry = (struct frame_entry *)malloc(sizeof(struct frame_entry));
    temp_frame_entry->physical_memory = kpage;
    //printf("physical memory : %p\n", ppage);
    struct hash_elem * find_elem = hash_find(&frame_hash, &temp_frame_entry->helem);
    
    if (find_elem == NULL){
        //printf("Can not find matched elem!\n\n");
        free(temp_frame_entry);
        return NULL;
    }
    //printf("Find matched elem!\n\n");
    struct frame_entry * find_entry = hash_entry(find_elem, struct frame_entry, helem);    
    free(temp_frame_entry);

    return find_entry;
}

/*
1. choose eviction frame
2. update sup_pagetable_entry 
3. deallocate current frame
*/
void
evict_frame (void){
    ASSERT(list_index != NULL);
    /*init iterator or after one cycle*/
    
    // unsigned frame_num = (unsigned) hash_size(&frame_hash);
    // unsigned iter_num = 0;

    struct frame_entry * iter_frame_entry;
    struct frame_entry * evict_frame_entry;

    /*no need to check protected in while loop. becasue it must be accessed*/
    while(true){
        iter_frame_entry = list_entry(list_index, struct frame_entry, lelem);
        list_index_move();
        /*보호 받는 중이면 통과. 아니면 변경*/
        if (iter_frame_entry->protected){
            ;
        }else if(pagedir_is_accessed(iter_frame_entry->t->pagedir, iter_frame_entry->physical_memory)){
            pagedir_set_accessed(iter_frame_entry->t->pagedir, iter_frame_entry->physical_memory, false);
        }else{
            evict_frame_entry = iter_frame_entry;
            break;
        }
    }

    /*
    1. swap table 빈공간 찾기
    2. sawp out하기
    3. supplemental page table에 기록하기
    4. PTE modify하기
    */
    /*1번 2번 동시에*/
    size_t swap_table_idx = swap_out(evict_frame_entry->physical_memory);
    if (swap_table_idx == -1){
        printf("PANIC!! swap is full");
    }
    
    /*update sup page table entry*/
    struct sup_pagetable_entry * sup_entry = sup_lookup(evict_frame_entry->t->sup_pagetable, evict_frame_entry->allocated_page);
    if(sup_entry==NULL){
        printf("PANIC!! Cannot find origin\n");
    }
    sup_entry->status = SWAPPED;
    sup_entry->physical_memory = NULL;
    sup_entry->swap_table_idx = swap_table_idx;

    /*TODO dirty bit control*/

    /*free frame entry, resource*/
    lock_acquire(&frame_lock);
    hash_delete(&frame_hash, &evict_frame_entry->helem);
    list_remove(&evict_frame_entry->lelem);    
    lock_release(&frame_lock);

    pagedir_clear_page(evict_frame_entry->t->pagedir, evict_frame_entry->allocated_page);
    palloc_free_page(evict_frame_entry->physical_memory);
    free(iter_frame_entry);
    //printf("finish\n");
}

/*
list_index기준으로 가장 멀리 있는 위치에 새 원소를 insert 해준다.
lock을 가진 상태로 들어와야 한다.
*/
void
list_insert_before_index(struct list_elem *elem){
    /*empty list, initialize*/
    if (list_empty(&frame_list)){
        list_push_front(&frame_list, elem);
        list_index_move();
    }else if (list_prev(list_index) == list_head(&frame_list)){
        list_push_front(&frame_list, elem);
    }else{
        list_insert(list_prev(list_index), elem);
    }
}

/*
list_index move to next index.
Get index circular
if next elem == list_end, get front_elem of list 
*/
void
list_index_move(void){
    ASSERT(list_size(&frame_list) != 0);
    if (list_index==NULL || list_index == list_end(&frame_list)){
        list_index = list_front(&frame_list);
    }else{
        list_index = list_next(list_index);
    }
}

/*
frame_hash_function by tid로 하려고 했으나... deallocate에서 구현 문제 발생
deallocate하려면 page만 가지고 어떤 hash elem일지 유추 가능해야함... list로 짤까?
physical memory로 hash시킴
*/
unsigned
frame_hash_func (const struct hash_elem *e, void *aux UNUSED){
    struct frame_entry *frame_entry = hash_entry (e, struct frame_entry, helem);
    return hash_bytes(&frame_entry->physical_memory, sizeof(frame_entry->physical_memory));
    //return hash_int((int)&frame_entry->physical_memory);
}

bool
frame_less_func (const struct hash_elem *a,
                 const struct hash_elem *b,
                 void *aux UNUSED){
    struct frame_entry *a_entry = hash_entry(a, struct frame_entry, helem);
    struct frame_entry *b_entry = hash_entry(b, struct frame_entry, helem);
    return a_entry->physical_memory < b_entry->physical_memory;
}
