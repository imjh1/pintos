#include "vm/frame.h"

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
    if(is_tail(check_frame)
      check_frame = 
    else
      check_frame = list_next(check_frame);
  }
}

