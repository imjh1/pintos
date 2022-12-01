#include "vm/swap.h"
#include "devices/block.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"

void swap_init()
{
  swap_bitmap = bitmap_create(1<<13);
}

void swap_in(size_t idx, void *paddr)
{
//  printf("swap_in start!, idx: %d\n", idx);

  struct block *swap_disk = block_get_role(BLOCK_SWAP);
  if(bitmap_test(swap_bitmap, idx)){
//    printf("bit test suc %d\n", idx);
    int blocks = PGSIZE / BLOCK_SECTOR_SIZE;
    for(int i=0; i<blocks; i++){
      block_read(swap_disk, blocks * idx + i, BLOCK_SECTOR_SIZE * i + paddr);
    }
    bitmap_reset(swap_bitmap, idx);
//	printf("swap fin\n");
  }
//  else
//    printf("fail\n");
}

size_t swap_out(void *paddr)
{
//  printf("swap_out start!\n");
 
  struct block *swap_disk=block_get_role(BLOCK_SWAP);
  //first fit에 따라 가장 처음으로 false를 나타내는 index를 가져옴.
  size_t swap_idx = bitmap_scan(swap_bitmap, 0, 1, false);
  if(BITMAP_ERROR != swap_idx)
    {
//	printf("swap_out idx = %d\n", swap_idx);
      int blocks = PGSIZE / BLOCK_SECTOR_SIZE;
      for(int i=0; i<blocks ; i++)
        {
          block_write(swap_disk, blocks * swap_idx+i, BLOCK_SECTOR_SIZE * i + paddr);
        }
      bitmap_set(swap_bitmap, swap_idx, true);
//      printf("swap_out suc\n");
    }
//  else
//    printf("swap_out error\n");

  return swap_idx;
}



