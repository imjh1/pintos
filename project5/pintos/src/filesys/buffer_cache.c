#include "filesys/buffer_cache.h"
#include "filesys/filesys.h"

#define NUM_CACHE 64

static int check_idx;	// for sec chance algoritm
static struct buffer_cache_entry cache[NUM_CACHE];

/* buffer cache initialization */
void buffer_cache_init (void)
{
  /* sec chance idx */
  check_idx = 0; 
  /* 각 buffer invalid로 init */
  for(int i=0; i<NUM_CACHE; i++)
    cache[i].valid_bit = false;
  /* lock init */
  lock_init(&buffer_cache_lock);
}

/* 모든 buffer flush */
void buffer_cache_terminate (void)
{
  buffer_cache_flush_all();
}

/* read from buffer cache */
void buffer_cache_read (block_sector_t sector, uint8_t *buffer, int chunk_size, off_t bytes_read, int sector_ofs)
{
  lock_acquire(&buffer_cache_lock);
  /* buffer entry 찾기 */
  struct buffer_cache_entry *bce = find_buffer_cache (sector);
  bce->reference_bit = true;	// reference bit 갱신
  memcpy (buffer + bytes_read, bce->buffer + sector_ofs, chunk_size); 

  lock_release(&buffer_cache_lock);  
}

/* write to buffer cache */
void buffer_cache_write (block_sector_t sector, uint8_t *buffer, int chunk_size, off_t bytes_written, int sector_ofs)
{
  lock_acquire(&buffer_cache_lock);
  /* buffer entry 찾기 */
  struct buffer_cache_entry *bce = find_buffer_cache (sector);
  bce->reference_bit = true;	// reference bit 갱신
  bce->dirty_bit = true;        // dirty bit 갱신
  memcpy (bce->buffer + sector_ofs, buffer + bytes_written, chunk_size);

  lock_release(&buffer_cache_lock);
}

/* buffer cache 찾아 return하는 함수 */
struct buffer_cache_entry *find_buffer_cache (block_sector_t sector)
{
  /* buffer_cache에 sector 존재하는 지 확인 */
  struct buffer_cache_entry *ret = buffer_cache_lookup (sector);
  if(ret != NULL)
    return ret;

  /* buffer cache에 없는 경우, empty buffer cache 확인 */
  ret = find_empty_buffer (sector);
  /* empty buffer 없는 경우, sec chance algoritm으로 buffer cache entry evict */
  if(ret == NULL)
    ret = buffer_cache_select_victim (sector);

  /* buffer cache에 block sector 저장 */
  block_read (fs_device, sector, ret->buffer);
  return ret;
}

/* buffer에 sector에 해당하는 buffer_cache 존재하는 지 확인 */
struct buffer_cache_entry *buffer_cache_lookup (block_sector_t sector)
{
  /* buffer cache table 탐색 */
  for(int i=0; i < NUM_CACHE; i++){
    /* block sector가 buffer cache에 있는 경우, 해당 buffer cache entry return */
    if(cache[i].disk_sector == sector && cache[i].valid_bit)
      return &cache[i];
  }

  return NULL;
}

/* empty buffer 찾아 return */
struct buffer_cache_entry *find_empty_buffer (block_sector_t sector)
{
  for(int i=0; i < NUM_CACHE; i++){
    /* empty cache */
    if(cache[i].valid_bit == false){
      cache[i].valid_bit = true;
      cache[i].reference_bit = true;
      cache[i].dirty_bit = false;
      cache[i].disk_sector = sector;
      return &cache[i];
    }
  }

  return NULL;
}

/* sec chance algorithm으로 buffer cache entry evict */
struct buffer_cache_entry *buffer_cache_select_victim (block_sector_t sector)
{
  struct buffer_cache_entry *evict_buffer;
  while (1) {
    /* reference bit가 false면 해당 buffer cache entry evict */
    if(cache[check_idx].reference_bit == false){
      evict_buffer = &cache[check_idx];
      break;
    }
    /* reference bit가 true면 
       해당 buffer cache entry의 reference bit false로 변경하고
       다음 buffer cache entry 확인 */
    cache[check_idx].reference_bit = false;
    check_idx = (check_idx + 1) % NUM_CACHE;
  }
  
  /* evict하는 buffer cache entry의 dirty bit가 true면 disk에 write */
  if(evict_buffer->dirty_bit)
    buffer_cache_flush_entry(evict_buffer);

  /* evict한 buffer cache entry에 "sector" 저장 */
  evict_buffer->valid_bit = true;
  evict_buffer->dirty_bit = false;
  evict_buffer->disk_sector = sector;
  check_idx = (check_idx + 1) % NUM_CACHE;
  return evict_buffer;
}

/* buffer cache entry disk에 write */
void buffer_cache_flush_entry(struct buffer_cache_entry* bce)
{
  block_write (fs_device, bce->disk_sector, bce->buffer);
  bce->dirty_bit = false; 
}

/* 모든 buffer cache entry의 dirty bit check하여 disk에 write */
void buffer_cache_flush_all(void)
{
  /* 모든 buffer cache flush */
  for(int i=0; i < NUM_CACHE; i++){
    /* valid하면서 dirty bit가 true인 entry disk에 write */
    if(cache[i].valid_bit && cache[i].dirty_bit)
      buffer_cache_flush_entry(&cache[i]);
  }
}

