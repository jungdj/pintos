#include "threads/palloc.h"
#include "lib/kernel/hash.h"

struct frame_entry
{
    struct hash_elem helem; /* for rotation */
    struct list_elem lelem;

    struct thread *t; /* assigned thread */

    void * physical_memory; /*allocated physical memory*/
    void * allocated_page; /*allocated page*/
    bool protected;
};

void frame_init(void);
void * allocate_new_frame(enum palloc_flags flag, void * upage);
void deallocate_frame(void * page);
struct frame_entry * lookup_frame(void * upage);
void evict_frame(void);