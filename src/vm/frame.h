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
	struct sup_page_table_entry* spte;
	struct hash_elem h_elem;
	void *kpage; // Save kernel virtual page address TODO: others use uint8_t as type, why??
	void *upage;
	bool pinned;
};

void frame_init (void);
struct frame_table_entry* get_frame_table_entry (void *kpage);
void * allocate_frame_and_pin (enum palloc_flags flags, void *upage, bool pinned);
void * allocate_frame (enum palloc_flags flags, void *upage);
void fte_update_pinned (void *kpage, bool pinned);
void fte_install_spte (void *kpage, struct sup_page_table_entry *spte);
void free_frame_with_lock (void *kpage);
void free_frame (void *addr);
void * select_victim_frame (void);
#endif /* vm/frame.h */
