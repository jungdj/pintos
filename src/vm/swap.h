#ifndef VM_SWAP_H
#define VM_SWAP_H

void swap_init (void);
bool swap_in (void *addr);
bool swap_out (void);
void read_from_disk (uint8_t *frame, int index);
void write_to_disk (uint8_t *frame, int index);

#endif /* vm/swap.h */
