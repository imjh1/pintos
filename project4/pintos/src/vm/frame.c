#include <list.h>
#include "vm/page.h"
#include "vm/frame.h"
#include "vm/swap.h"

void sec_chance_init (void)
{
  list_init(&sec_chance_list);
  check_frame = NULL; 
}

void sec_chance_insert (struct frame *f)
{
  list_push_back(&sec_chance_list, &f->elem);
}
  
void sec_chance_delete (struct frame *f)
{
  if(check_frame == &f->elem){
    check_frame = next_frame ();
  }

  list_remove(&f->elem);
}

struct list_elem *next_frame (void)
{
  if(list_empty(&sec_chance_list))
    return NULL;

  if(check_frame == NULL || check_frame == list_end(&sec_chance_list))
    return list_begin(&sec_chance_list);

  return list_next(check_frame);
}

void evict_frame (void)
{
  struct frame *victim = NULL;
  check_frame = next_frame ();

  while(check_frame){
    victim = list_entry(check_frame, struct frame, elem);
    /* accessed bit가 0 */
    if(!(victim->sp->vaddr >= 0x8040000 && victim->sp->vaddr <= 0x8060000) && !pagedir_is_accessed(victim->owner->pagedir, victim->sp->vaddr))
      break;
    pagedir_set_accessed(victim->owner->pagedir, victim->sp->vaddr, false);
    check_frame = next_frame ();
  }

  if(victim == NULL)
    return;
  
  if(pagedir_is_dirty(victim->owner->pagedir, victim->sp->vaddr))
    victim->sp->swap_slot = swap_out(victim->paddr);

//  printf("%p\n", victim->sp->vaddr);

  victim->sp->is_loaded = false; 
 
  sec_chance_delete(victim);
  pagedir_clear_page(victim->owner->pagedir, pg_round_down(victim->sp->vaddr));
  palloc_free_page(victim->paddr);
  free(victim);
}
