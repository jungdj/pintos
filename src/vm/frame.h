#include "threads/palloc.h"
#include "lib/kernel/hash.h"

struct frame_entry
{
    struct hash_elem elem; /* for rotation */
    struct thread *t; /* assigned thread */

    unsigned physical_memory; /*allocated physical memory*/
    unsigned allocated_page; /*allocated page*/
};

void frame_init();
void * allocate_frame(enum palloc_flags flag);
void deallocate_frame(void * page);
