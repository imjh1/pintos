#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "threads/thread.h"
#include "threads/interrupt.h"

struct lock file_read_write;

void syscall_init (void);
/* invalid pointer check */
void check_address(struct intr_frame *f, int argc);
/* Project 2 */
void halt (void);
void exit (int status);
tid_t exec (const char *file);
int wait (tid_t pid);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned length);
int write (int fd, const void *buffer, unsigned length);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);
/* Project 3 */
//mapid_t mmap (int fd, void *addr);
//void munmap (mapid_t);
/* Project 4 */
//bool chdir (const char *dir);
//bool mkdir (const char *dir);
//bool readdir (int fd, char name[READDIR_MAX_LEN + 1]);
//bool isdir (int fd);
//int inumber (int fd);
/* Project 2 additional */
int fibonacci (int n);
int max_of_four_int (int a, int b, int c, int d);
#endif /* userprog/syscall.h */
