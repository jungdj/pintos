#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

void swap_init (void);
void swap_in (size_t swap_index, void *page);
size_t swap_out (void *addr);
void free_swap_slot (size_t swap_index);

#endif /* vm/swap.h */
