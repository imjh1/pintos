#include "filesys/buffer_cache.h"
#include "filesys/filesys.h"

#define NUM_CACHE 64

static int check_idx;	// for sec chance algoritm
static struct buffer_cache_entry cache[NUM_CACHE];

void buffer_cache_init (void)
{
  check_idx = 0; 
  for(int i=0; i<NUM_CACHE; i++)
    cache[i].valid_bit = false;
  lock_init(&buffer_cache_lock);
}

void buffer_cache_terminate (void)
{
  printf("2\n");
  buffer_cache_flush_all();
}

void buffer_cache_read (block_sector_t sector, uint8_t *buffer, int chunk_size, off_t bytes_read, int sector_ofs)
{
  lock_acquire(&buffer_cache_lock);
  /* buffer entry 찾기 */
  struct buffer_cache_entry *bce = find_buffer_cache (sector);
  bce->reference_bit = true;
  
  memcpy (buffer + bytes_read, bce->buffer + sector_ofs, chunk_size); 

  lock_release(&buffer_cache_lock);  
}

void buffer_cache_write (block_sector_t sector, uint8_t *buffer, int chunk_size, off_t bytes_written, int sector_ofs)
{
  lock_acquire(&buffer_cache_lock);
  /* buffer entry 찾기 */
  struct buffer_cache_entry *bce = find_buffer_cache (sector);
  bce->reference_bit = true;  
 
  memcpy (bce->buffer + sector_ofs, buffer + bytes_written, chunk_size);
  bce->dirty_bit = true;
  lock_release(&buffer_cache_lock);
}

/* buffer cache 찾아 return하는 함수 */
struct buffer_cache_entry *find_buffer_cache (block_sector_t sector)
{
  /* buffer에 존재하는 지 확인 */
  struct buffer_cache_entry *ret = buffer_cache_lookup (sector);
  if(ret != NULL)
    return ret;

  /* empty buffer 존재하는 지 확인 */
  ret = find_empty_buffer (sector);
  /* empty buffer 없는 경우, sec chance algoritm으로 buffer entry evict */
  if(ret == NULL)
    ret = buffer_cache_select_victim (sector);

  block_read (fs_device, sector, ret->buffer);
  return ret;
}

/* buffer에 sector에 해당하는 buffer_cache 존재하는 지 확인 */
struct buffer_cache_entry *buffer_cache_lookup (block_sector_t sector)
{
  for(int i=0; i<NUM_CACHE; i++){
    if(cache[i].disk_sector == sector && cache[i].valid_bit)
      return &cache[i];
  }

  return NULL;
}

/* empty buffer return */
struct buffer_cache_entry *find_empty_buffer (block_sector_t sector)
{
  for(int i=0; i<NUM_CACHE; i++){
    if(cache[i].valid_bit == false){
      cache[i].valid_bit = true;
      cache[i].dirty_bit = false;
      cache[i].disk_sector = sector;
      return &cache[i];
    }
  }

  return NULL;
}

struct buffer_cache_entry *buffer_cache_select_victim (block_sector_t sector)
{
  struct buffer_cache_entry *evict_buffer;
  while(1){
    /* reference bit가 false인 buffer evict */
    if(cache[check_idx].reference_bit == false){
      evict_buffer = &cache[check_idx];
      break;
    }
    cache[check_idx].reference_bit = false;
    check_idx = (check_idx + 1) % NUM_CACHE;
  }
  
  /* dirty bit가 true면 disk write */
  if(evict_buffer->dirty_bit)
    buffer_cache_flush_entry(evict_buffer);

  evict_buffer->valid_bit = true;
  evict_buffer->dirty_bit = false;
  evict_buffer->disk_sector = sector;
  check_idx = (check_idx + 1) % NUM_CACHE;
  return evict_buffer;
}

void buffer_cache_flush_entry(struct buffer_cache_entry* bce)
{
  block_write (fs_device, bce->disk_sector, bce->buffer);
  bce->dirty_bit = false; 
}

void buffer_cache_flush_all(void)
{
  /* 모든 buffer cache flush */
  for(int i=0; i<NUM_CACHE; i++){
    if(cache[i].valid_bit && cache[i].dirty_bit)
      buffer_cache_flush_entry(&cache[i]);
  }
}

