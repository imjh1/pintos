#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/exception.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "devices/input.h"
#include "devices/shutdown.h"

static void syscall_handler (struct intr_frame *);

/* invalid pointer check */
void check_address(struct intr_frame *f, int argc);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  size_t word = sizeof(uint32_t);
  int fd;
  void *buffer;
  unsigned length;
  
  /* f->esp : system call number
     f->esp + word * x : argument (x-1) */

  struct thread *cur = thread_current();
  /* check pointer to kernel address space */
  if(is_kernel_vaddr(f->esp))
    exit(-1);
  /* check NULL pointer */
  if((void*)(f->esp) == NULL)
    exit(-1);
  /* check Unmapped Virtual Memory */
  if(pagedir_get_page(cur->pagedir, (void*)(f->esp)) == NULL)
    exit(-1);

  int syscall_num = (*(uint32_t *)(f->esp));

  switch (syscall_num){
    case SYS_HALT:                   /* Halt the operating system. */
	halt();
        break;
    case SYS_EXIT:                   /* Terminate this process. */
	// syscall 1
	check_address(f, 1);
	exit(*(uint32_t *)(f->esp + word));
	break;
    case SYS_EXEC:                   /* Start another process. */
	// syscall 1
	check_address(f, 1);
	f->eax = exec((char *)*(uint32_t *)(f->esp + word));
	break;
    case SYS_WAIT:                   /* Wait for a child process to die. */
	// syscall 1
	check_address(f, 1);
	f->eax = wait((tid_t)*(uint32_t *)(f->esp + word));
	break;
    case SYS_CREATE:                 /* Create a file. */
	//create
	break;
    case SYS_REMOVE:                 /* Delete a file. */
	//remove
	break;
    case SYS_OPEN:                   /* Open a file. */
	//open
	break;
    case SYS_FILESIZE:               /* Obtain a file's size. */
	//filesize
	break;
    case SYS_READ:                   /* Read from a file. */
	// syscall 3
	check_address(f, 3);
	fd = (int)*(uint32_t *)(f->esp + word);
	buffer = (void *)*(uint32_t *)(f->esp + word * 2);
	length = (unsigned)*(uint32_t *)(f->esp + word * 3);

	f->eax = read(fd, buffer, length);
	break;
    case SYS_WRITE:                  /* Write to a file. */
	// syscall 3   
	check_address(f, 3);     
	fd = (int)*(uint32_t *)(f->esp + word);			// arg0
	buffer = (void *)*(uint32_t *)(f->esp + word * 2);	// arg1
	length = (unsigned)*(uint32_t *)(f->esp + word * 3);	// arg2

	f->eax = write(fd, buffer, length);
	break;
    case SYS_SEEK:                   /* Change position in a file. */
	//seek
	break;
    case SYS_TELL:                   /* Report current position in a file. */
	//tell
	break;
    case SYS_CLOSE:                  /* Close a file. */
	//close
	break;

    /* Project 3 and optionally project 4. */
    case SYS_MMAP:                   /* Map a file into memory. */
	break;
    case SYS_MUNMAP:                 /* Remove a memory mapping. */
	break;

    /* Project 4 only. */
    case SYS_CHDIR:                  /* Change the current directory. */
	break;
    case SYS_MKDIR:                  /* Create a directory. */
	break;
    case SYS_READDIR:                /* Reads a directory entry. */
	break;
    case SYS_ISDIR:                  /* Tests if a fd represents a directory. */
	break;
    case SYS_INUMBER:                /* Returns the inode number for a fd. */
	break;

    /* Project 2 additional */
    case SYS_FIBO:
	/* syscall 1 */
	check_address(f, 1);
	f->eax = fibonacci((int)*((uint32_t *)(f->esp + word)));
	break;
    case SYS_MAXOF4:
	/* syscall 4 */
	check_address(f, 4);
	f->eax = max_of_four_int((int)*((uint32_t *)(f->esp + word)), (int)*((uint32_t *)(f->esp + word*2)), (int)*((uint32_t *)(f->esp + word*3)), (int)*((uint32_t *)(f->esp + word*4)));
	break;
  }
}

void halt (void){
  shutdown_power_off();
}

void exit (int status){
  struct thread *cur = thread_current();
  cur->exit_status = status;

  // process termination messeage
//  cur->exit_status = status;
//  printf("%s: exit(%d)\n", cur->name, status);
  thread_exit();
}

tid_t exec (const char *file){
  tid_t ret = -1;

//  if(is_kernel_vaddr(file)){
//	  return ret;
//  }
//  if((void*)(file) == NULL){
//	  return ret;
//  }
  

  ret = process_execute(file);


  return ret;
}

int wait (tid_t pid){
  return process_wait(pid);
}
/*
bool create (const char *file, unsigned initial_size){
	return true;
}

bool remove (const char *file){
	return true;
}

int open (const char *file){
	return -1;
}

int filesize (int fd){
	return -1;
}
*/
int read (int fd, void *buffer, unsigned length){
  int ret = -1;

  if(fd == STDIN_FILENO){
    unsigned read_bytes = 0;

    while(read_bytes < length){
      uint8_t key = input_getc();
      if(key == '\0')
        break;
      read_bytes++;      
    }

    ret = read_bytes;
  }
  return ret;
}

int write (int fd, void *buffer, unsigned length)
{
  if(fd == STDOUT_FILENO){    
    putbuf(buffer, length);
    return length;
  }
  return -1;
}
/*
void seek (int fd, unsigned position){
	
}

unsigned tell (int fd){
	return 0;
}

void close (int fd){
	
}

mapid_t mmap (int fd, void *addr){
	return -1;
}

void munmap (mapid_t){

}

bool chdir (const char *dir){
	return true;
}

bool mkdir (const char *dir){
	return true;
}

bool readdir (int fd, char name[READDIR_MAX_LEN + 1]){
	return true;
}

bool isdir (int fd){
	return -1;
}

int inumber (int fd){
	return -1;
}
*/
int fibonacci (int n){
  if(n <= 0)
    return 0;

  int f_1 = 1, f_2 = 1;

  for(int i = 3; i <= n; i++){
    int tmp = f_2;
    f_2 += f_1;
    f_1 = tmp;
  }

  return f_2;
}

int max_of_four_int (int a, int b, int c, int d){
  a = a > b ? a : b;
  c = c > d ? c : d;
  return a > c ? a : c;
}

void check_address(struct intr_frame *f, int argc)
{
  size_t word = sizeof(uint32_t);
  struct thread *cur = thread_current();

  for(int i=1; i<=argc; i++){
    /* Pointer to kernel address space */
    if(is_kernel_vaddr(f->esp + word * i))
      exit(-1);
    /* NULL pointer */
    if((void*)(f->esp + word * i) == NULL)
      exit(-1);
    /* Unmapped Virtual Memory */
    if(pagedir_get_page(cur->pagedir, (void*)(f->esp + word * i)) == NULL)
      exit(-1);
    }
}

