#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include <list.h>
#include "threads/thread.h"
#include "threads/vaddr.h"

/* page fault handling을 위한 supplement page */
struct supplement_page {
  void *vaddr; 		// page의 virtual address
  bool writable;	// writable 여부
  struct hash_elem elem;// supplement page는 hash를 통해 관리
  size_t swap_slot;	// disk swap을 위한 swap index
};

void sp_init (struct hash *spt);
bool sp_insert (struct hash *spt, struct supplement_page *sp);
bool sp_delete (struct hash *spt, struct supplement_page *sp);
struct supplement_page *sp_find (void *vaddr);
void sp_destroy (struct hash *spt);

#endif
