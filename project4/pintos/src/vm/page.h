#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include <list.h>
#include "threads/thread.h"
#include "threads/vaddr.h"

struct supplement_page {
  void *vaddr; 
  bool writable;
  bool is_loaded;
  struct hash_elem elem;

  size_t swap_slot;
};

void sp_init (struct hash *spt);
bool sp_insert (struct hash *spt, struct supplement_page *sp);
bool sp_delete (struct hash *spt, struct supplement_page *sp);
struct supplement_page *sp_find (void *vaddr);
void sp_destroy (struct hash *spt);

#endif
