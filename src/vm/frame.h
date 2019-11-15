#include "threads/palloc.h"
#include "lib/kernel/hash.h"

struct frame_entry
{
    struct hash_elem elem; /* for rotation */
    struct thread *t; /* assigned thread */

    void * physical_memory; /*allocated physical memory*/
    void * allocated_page; /*allocated page*/
};

void frame_init(void);
void * allocate_new_frame(enum palloc_flags flag, void * upage);
void deallocate_frame(void * page);
struct frame_entry * lookup_frame(void * upage);
