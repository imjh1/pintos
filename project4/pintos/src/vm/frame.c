#include <list.h>
#include "vm/page.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "userprog/pagedir.h"

void sec_chance_init (void)
{
  list_init(&sec_chance_list);
  check_frame = NULL; 
}

/* frame을 list의 뒤에 삽입 */
void sec_chance_insert (struct frame *f)
{
  list_push_back(&sec_chance_list, &f->elem);
}

/* list에서 frame 제거하는 함수 */  
void sec_chance_delete (struct frame *f)
{
  /* check frame이 제거할 frame을 가리킨다면, next_frame() 호출 */
  if(check_frame == &f->elem){
    check_frame = next_frame ();
  }

  list_remove(&f->elem);
}

/* Circular list의 다음 element를 return하는 함수 */
struct list_elem *next_frame (void)
{
	
  if(list_empty(&sec_chance_list)){
    return NULL;
  }
  
  /* check_frame이 초기화되지 않았거나, list의 마지막 element를 가리킬 경우
     list의 첫번째 element를 가리키도록 함 */
  if(check_frame == NULL || check_frame == list_rbegin(&sec_chance_list))
    return list_begin(&sec_chance_list);

  return list_next(check_frame);
}

/* second chance algoritm을 통해 frame eviction */
void evict_frame (void)
{
  struct frame *victim = NULL;
  check_frame = next_frame ();

  while(check_frame){
    victim = list_entry(check_frame, struct frame, elem);
    /* accessed bit가 0이라면, 해당 frame evict */
    if(!(victim->sp->vaddr >= 0x8040000 &&victim->sp->vaddr <= 0x8060000) && !pagedir_is_accessed(victim->owner->pagedir, victim->sp->vaddr))
      break;
    /* accessed bit가 1이라면, accessed bit 0으로 변경 후 다음 frame check */
    pagedir_set_accessed(victim->owner->pagedir, victim->sp->vaddr, false);
    check_frame = next_frame ();
  }

  if(victim == NULL)
    return;

  /* victim page swap out */
  victim->sp->swap_slot = swap_out(victim->paddr);
  /* victim frame을 physical memory로부터 제거 */
  sec_chance_delete(victim);
  pagedir_clear_page(victim->owner->pagedir, victim->sp->vaddr);
  palloc_free_page(victim->paddr);
  free(victim);
}
