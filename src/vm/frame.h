#include <stdbool.h>
#include <stdint.h>
#include <hash.h>
#include "vm/page.h"

#ifndef VM_FRAME_H
#define VM_FRAME_H

struct frame_table_entry
{
	uint32_t* frame;
	struct thread* owner;
	struct sup_page_table_entry* spte; // Needed?
	struct hash_elem h_elem;
	void *page; // Save kernel virtual page address TODO: others use uint8_t as type, why??
};

void frame_init (void);
void *allocate_frame (enum palloc_flags flags, void *addr);
void free_frame (void *addr);

#endif /* vm/frame.h */
