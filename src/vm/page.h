#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <hash.h>
#include "threads/synch.h"
#include "threads/palloc.h"

/*
 * Do supplementary page tables need supplementary page dir table?
 * Maybe yes or maybe not.
 * As sup_pages don't need to be grouped as the same way pages are
 * grouped, supplementary page tables don't need sup_page_dir
struct sup_pagedir_table
{
    struct hash sup_page_table;
    struct lock sup_page_table_lock;
};

uint32_t * sup_pagedir_create (void);
struct hash* sup_page_create (void);

uint32_t *
sup_pagedir_create (void)
{
  struct sup_pagedir_table *spdt = malloc (sizeof (struct sup_pagedir_table));
  return spdt;
}

void
sup_pagedir_destroy (void *pagedir)
{
  // TODO: destroy sup pagedir table
}
*/

struct sup_page_table_entry
{
	uint32_t *user_vaddr;
	uint32_t *frame;

	// swap_key TODO: swap key if swapped
	// file_key TODO: file key if stored in file

	uint64_t access_time;

	struct hash_elem h_elem;

	bool dirty;
	bool accessed;
};

struct hash* sup_page_create (void);
void sup_page_destroy (struct hash *sup_page_table);

bool sup_page_install_frame (struct hash *sup_page_table, void *upage, void *kpage);
#endif /* vm/page.h */
