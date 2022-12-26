#ifndef FILESYS_BUFFER_CACHE_H
#define FILESYS_BUFFER_CACHE_H

#include "threads/thread.h"
#include "devices/block.h"
#include "filesys/off_t.h"
#include "lib/stdbool.h"

struct buffer_cache_entry{
  bool valid_bit;	// valid entry ?
  bool reference_bit;	// referenced ?
  bool dirty_bit;	// modified ? 
  block_sector_t disk_sector;
  uint8_t buffer[BLOCK_SECTOR_SIZE];	// 512 * 1B
};

struct lock buffer_cache_lock;

void buffer_cache_init (void);
void buffer_cache_terminate (void);
void buffer_cache_read (block_sector_t sector, uint8_t *buffer, int chunk_size, off_t bytes_read, int sector_ofs);
void buffer_cache_write (block_sector_t sector, uint8_t *buffer, int chunk_size, off_t bytes_written, int sector_ofs);
struct buffer_cache_entry *find_buffer_cache (block_sector_t sector);
struct buffer_cache_entry *buffer_cache_lookup (block_sector_t sector);
struct buffer_cache_entry *find_empty_buffer (block_sector_t sector);
struct buffer_cache_entry *buffer_cache_select_victim (block_sector_t sector);
void buffer_cache_flush_entry(struct buffer_cache_entry* bce);
void buffer_cache_flush_all(void);

#endif
