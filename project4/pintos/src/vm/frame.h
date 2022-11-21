#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <hash.h>
#include <list.h>
#include "threads/thread.h"
#include "threads/vaddr.h"

struct list sec_chance_list;
struct list_elem *check_frame;

struct frame {
  void *paddr;
  struct supplement_page *sp;
  struct thread *owner;
  struct list_elem elem;
};

void sec_chance_init (void);
void sec_chance_insert (struct frame *f);
void sec_chance_delete (struct frame *f);

#endif

