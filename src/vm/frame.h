#include "threads/palloc.h"
#include "lib/kernel/hash.h"
enum page_status{
    ON_FRAME, /* On main memroy*/
    SWAPPED, /* On Swapped memory*/
    ON_DISK /* on disk */
};

struct frame_entry
{
    struct hash_elem elem; /* for rotation */
    struct thread *t; /* assigned thread */

    void * physical_memory; /*allocated physical memory*/
    void * allocated_page; /*allocated page*/
    bool protected;
};

void frame_init(void);
void * allocate_new_frame(enum palloc_flags flag, void * upage);
void deallocate_frame(void * page);
struct frame_entry * lookup_frame(void * upage);
