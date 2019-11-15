#include "lib/kernel/hash.h"
#include "threads/pte.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "frame.h"
#include <stdio.h>
#include <list.h>
#include <stdint.h>

static struct hash frame_hash;
static struct lock frame_lock;

unsigned frame_hash_func(const struct hash_elem *e, void *aux);
bool frame_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux);

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
    
    /* if no new_page?? --> evcition*/
    struct frame_entry * frame_entry = malloc(sizeof(struct frame_entry));
    /* if malloc failed?? */
    frame_entry->t = thread_current();
    frame_entry->allocated_page = upage;
    frame_entry->physical_memory = new_page;
    
    /* lock before modifying hash */
    lock_acquire(&frame_lock);
    hash_insert(&frame_hash, &frame_entry->elem);
    lock_release(&frame_lock);
    return (void *)new_page; 
}


/*
마찬가지로 proceess.c 에 있는 palloc_free_page와 유사하게 구현
page를 free하고 frame table에서도 제거
*/
void
deallocate_frame(void * upage){
    /*temp frame entry to find real frame_entry*/
    struct frame_entry * temp_frame_entry = (struct frame_entry *)malloc(sizeof(struct frame_entry));
    /* if malloc failed?? */
    temp_frame_entry->allocated_page = upage;

    struct hash_elem * existed_elem = hash_find(&frame_hash, &temp_frame_entry->elem);
    free(temp_frame_entry);
    if (existed_elem == NULL){
        return false;
    }

    struct frame_entry * existed_frame_entry = hash_entry(existed_elem, struct frame_entry, elem);
    palloc_free_page(upage);
    
    lock_acquire(&frame_lock);
    hash_delete(&frame_hash, &existed_frame_entry->elem);
    lock_release(&frame_lock);

    free(existed_frame_entry);
}

struct frame_entry *
lookup_frame(void * ppage){
    struct frame_entry * temp_frame_entry = (struct frame_entry *)malloc(sizeof(struct frame_entry));
    temp_frame_entry->physical_memory = ppage;

    struct hash_elem * find_elem = hash_find(&frame_hash, &temp_frame_entry->elem);
    
    if (find_elem == NULL){
        free(temp_frame_entry);
        return NULL;
    }
    
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
    return hash_int((int)&frame_entry->physical_memory);
}

bool
frame_less_func (const struct hash_elem *a,
                 const struct hash_elem *b,
                 void *aux UNUSED){
    struct frame_entry *a_entry = hash_entry(a, struct frame_entry, elem);
    struct frame_entry *b_entry = hash_entry(b, struct frame_entry, elem);
    return a_entry->physical_memory < b_entry->physical_memory;
}