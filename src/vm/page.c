#include "debug.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "page.h"
#include "vm/swap.h"
#include "lib/kernel/hash.h"
#include <stdio.h>

/* about supplementary page*/

/*Todo: control hash insert, delete with lock*/

unsigned sup_pagetable_hash_func (const struct hash_elem *e, void *aux UNUSED);
bool sup_pagetable_less_func (const struct hash_elem *a,
                            const struct hash_elem *b,
                            void *aux UNUSED);
void sup_pagetable_destroy_func(struct hash_elem *e, void *aux UNUSED);

/*
Init sub page table in current thread.
It will call in thread init.
*/

void
sup_pagetable_create(struct thread *t){
    struct hash * subpage = (struct hash *) malloc(sizeof(struct hash));
    hash_init(subpage, sup_pagetable_hash_func, sup_pagetable_less_func, NULL);
    t->sup_pagetable = subpage;
}

/*
Destory sub page table in current thread.
It will call in th exit.
*/
void
sup_pagetable_destroy(void){
    // ASSERT (thread_current()->sup_pagetable != NULL);
    struct hash * suppage = thread_current()->sup_pagetable;
    hash_destroy(suppage, sup_pagetable_destroy_func); // kill all element of hash
    free(thread_current()->sup_pagetable);
    /* if 이정도면 아예 삭제된 것이 맞을까? */
}

/*
sup_pagetable에 한 page set에(one entry) 대한 정보 추가. 
upage가 
*/
bool
sup_pagetable_set_page(struct thread *t, void* upage, void* ppage){
    struct sup_pagetable_entry * sup_entry = (struct sup_pagetable_entry *)malloc(sizeof(struct sup_pagetable_entry));

    ASSERT (sup_entry != NULL);

    sup_entry->status = ON_FRAME;
    sup_entry->allocated_page = upage;
    sup_entry->physical_memory = ppage;
    sup_entry->swap_table_idx = NULL;

    struct hash_elem *hash_elem;
    hash_elem = hash_insert(t->sup_pagetable, &sup_entry->elem);
    if (hash_elem == NULL){
        return true;
    }else{
        printf("same sup_page_entry error!! \n\n");
        free(sup_entry);
        return false;
    }
}

/*
current thread의 sup pagedir에서 요청된 upage에 대한 entry 생성
clear 조건은 아직 생각을 못했음. 언제 clear 해야 하지 -> sup page에서 찾아져서 phsical로 옮겨졌을때
*/
bool
sup_pagetable_clear_page (struct hash * sup_pagetable, void* upage) 
{
    struct sup_pagetable_entry * existed_pagetable_entry = sup_lookup(sup_pagetable, upage);
    ASSERT (existed_pagetable_entry != NULL);
    hash_delete(sup_pagetable, &existed_pagetable_entry->elem);
    if(existed_pagetable_entry->status == SWAPPED)
        swap_free(existed_pagetable_entry->swap_table_idx);
    free(existed_pagetable_entry);
    return true;
}

struct sup_pagetable_entry *
sup_lookup(struct hash * sup_pagetable, void* upage){

    ASSERT (sup_pagetable != NULL);

    struct sup_pagetable_entry * temp_sup_entry = (struct sup_pagetable_entry *)malloc(sizeof(struct sup_pagetable_entry *));
    temp_sup_entry->allocated_page = upage;
    struct hash_elem * find_elem = hash_find(sup_pagetable, &temp_sup_entry->elem);
    if (find_elem == NULL){
        free(temp_sup_entry);
        return NULL;
    }
    free(temp_sup_entry);
    return hash_entry(find_elem, struct sup_pagetable_entry, elem);
}

/*
two functions about hash are quite similar to frame's 
*/
unsigned
sup_pagetable_hash_func (const struct hash_elem *e, void *aux UNUSED){
    struct sup_pagetable_entry *sup_entry = hash_entry (e, struct sup_pagetable_entry, elem);
    return hash_int((int)sup_entry->allocated_page);
}

bool
sup_pagetable_less_func (const struct hash_elem *a,
                 const struct hash_elem *b,
                 void *aux UNUSED){
    struct sup_pagetable_entry *a_entry = hash_entry(a, struct sup_pagetable_entry, elem);
    struct sup_pagetable_entry *b_entry = hash_entry(b, struct sup_pagetable_entry, elem);
    return a_entry->allocated_page < b_entry->allocated_page;
}

/*
This function for destroy hash(not hash elem).
when destroy subpage, free all sup_pagetable_entry with this fucntion 
This function was created with reference to hash_action function in hash.h.
*/
void sup_pagetable_destroy_func(struct hash_elem *e, void *aux UNUSED){
    struct sup_pagetable_entry * sup_entry = hash_entry (e, struct sup_pagetable_entry, elem);
    if(sup_entry->status == SWAPPED)
        swap_free(sup_entry->swap_table_idx);
    free(sup_entry);
}