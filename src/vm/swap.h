#include "lib/kernel/bitmap.h"

void swap_init(void);
size_t swap_out(void* physical_memory);
void swap_in(size_t idx, void * physical_memory);
void swap_free(size_t index);