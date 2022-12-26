#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <hash.h>
#include <list.h>

struct block *swap_disk;
int *swap_table;

void swap_init(void);
void swap_in(size_t idx, void *paddr);
size_t swap_out(void *paddr);

#endif
