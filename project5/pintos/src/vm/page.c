#include "vm/page.h"
#include "threads/vaddr.h"

static unsigned spt_hash_func (const struct hash_elem *e,void *aux)
{
  /* hash_int()를 이용해서 supplement_page의member인 vaddr에 대한 해시값을 구하고 return */
  return hash_int (hash_entry(e, struct supplement_page, elem)->vaddr);
}

static bool spt_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux)
{
/* hash_entry()로 각각의 element에 대한 vm_entry 구조체를 얻은
후 vaddr 비교 (b가 크다면 true, a가 크다면 false */
  struct supplement_page *spa = hash_entry(a, struct supplement_page, elem);
  struct supplement_page *spb = hash_entry(b, struct supplement_page, elem);

  return spa->vaddr < spb->vaddr;
}

static void spt_destruct_func (struct hash_elem *e, void *aux)
{

}

void sp_init (struct hash *spt)
{
  hash_init(spt, spt_hash_func, spt_less_func, NULL);
}

bool sp_insert (struct hash *spt, struct supplement_page *sp)
{  
  sp->swap_slot = SIZE_MAX;
  if(hash_insert(spt, &sp->elem) == NULL)
    return true;
  return false;
}

bool sp_delete (struct hash *spt, struct supplement_page *sp)
{
  if(hash_delete(spt, &sp->elem) != NULL)
    return true;
  return false;
}

struct supplement_page *sp_find (void *vaddr)
{
  struct thread *cur = thread_current ();
  struct supplement_page sp;
  
  sp.vaddr = pg_round_down(vaddr);
  struct hash_elem *e = hash_find(&cur->spt, &sp.elem);

  if(e == NULL)
    return NULL;
  return hash_entry(e, struct supplement_page, elem);
}

void sp_destroy (struct hash *spt)
{

}
