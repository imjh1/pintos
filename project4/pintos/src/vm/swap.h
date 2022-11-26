#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <hash.h>
#include <list.h>
#include <bitmap.h>

struct bitmap *swap_bitmap;

void swap_init();
void swap_in(size_t idx, void *paddr);
size_t swap_out(void *paddr);

#endif
