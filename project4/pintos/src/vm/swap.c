#include "vm/swap.h"
#include "devices/block.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"

#define blocks (PGSIZE / BLOCK_SECTOR_SIZE)
#define swap_table_size 1024

void swap_init (void)
{
  swap_disk = block_get_role(BLOCK_SWAP);
  swap_table = (int *)malloc(sizeof(int) * swap_table_size);
  for(int i=0; i<swap_table_size; i++)
    swap_table[i] = -1;
}

void swap_in(size_t idx, void *paddr)
{
  /* page read from swap disk */
  for(int i=0; i<blocks; i++)
    block_read(swap_disk, blocks * idx + i, BLOCK_SECTOR_SIZE * i + paddr);
  /* swap_slot 비우기 */
  swap_table[idx] = -1;
}

size_t swap_out(void *paddr)
{
  size_t swap_idx = SIZE_MAX;
  /* swap table의 비어있는 swap slot 탐색 */
  for(int i=0; i<swap_table_size; i++) {
    /* 비어있는 swap slot 찾은 경우 */
    if(swap_table[i] == -1) {
      swap_idx = i;
      /* write page to swap disk */
      for(int j=0; j<blocks; j++)
        block_write(swap_disk, blocks * swap_idx + j, BLOCK_SECTOR_SIZE * j + paddr);

      /* swap slot 채우기 */
      swap_table[i] = 1;

      break;
    }
  }

  return swap_idx;
}
