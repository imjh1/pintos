#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <hash.h>
#include <list.h>
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"

struct list sec_chance_list;
struct list_elem *check_frame;

struct frame {
  void *paddr;			// physical address
  struct supplement_page *sp;	// 해당 frame에 mapping된 page
  struct thread *owner;		// page의 owner
  struct list_elem elem;	// sec_chance list에서 탐색을 위한 list_elem
};

void sec_chance_init (void);
void sec_chance_insert (struct frame *f);
void sec_chance_delete (struct frame *f);
struct list_elem *next_frame (void);
void evict_frame (void);

#endif
