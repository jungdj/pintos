#include <stdbool.h>
#include <stdint.h>
#include "vm/page.h"

#ifndef VM_FRAME_H
#define VM_FRAME_H

struct frame_table_entry
{
	uint32_t* frame;
	struct thread* owner;
	struct sup_page_table_entry* spte;
};

void frame_init (void);
bool allocate_frame (void *addr);

#endif /* vm/frame.h */
