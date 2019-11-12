#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <stdint.h>
#include <stdbool.h>

struct sup_page_table_entry
{
	uint32_t* user_vaddr;
	uint64_t access_time;

	bool dirty;
	bool accessed;
};

void page_init (void);
struct sup_page_table_entry *allocate_page (void *addr);

#endif /* vm/page.h */
