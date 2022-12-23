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
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "filesys/directory.h"

static void syscall_handler (struct intr_frame *);

void check_address(struct intr_frame *f, int argc)
{
  size_t word = sizeof(uint32_t);
  struct thread *cur = thread_current();

  for(int i=1; i<=argc; i++){
    /* Pointer to kernel address space */
    if(is_kernel_vaddr(f->esp + word * i))
      exit(-1);
    /* NULL pointer */
    if((void *)(f->esp + word * i) == NULL)
      exit(-1);
    /* Unmapped Virtual Memory */
    if(pagedir_get_page(cur->pagedir, (void*)(f->esp + word * i)) == NULL)
      exit(-1);
  }
}

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&file_read_write);	// file_read_write lock 변수 초기화
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
	// syscall 2
	check_address(f, 2);
	length = (unsigned)*(uint32_t *)(f->esp + word * 2);

	f->eax = create((const char*)*(uint32_t *)(f->esp + word), length);
	break;
    case SYS_REMOVE:                 /* Delete a file. */
	// syscall 1
	check_address(f, 1);

	f->eax = remove((const char*)*(uint32_t *)(f->esp + word));
	break;
    case SYS_OPEN:                   /* Open a file. */
	// syscall 1
	check_address(f, 1);

	f->eax = open((const char*)*(uint32_t *)(f->esp + word));
	break;
    case SYS_FILESIZE:               /* Obtain a file's size. */
	// syscall 1
	check_address(f, 1);
	fd = (int)*(uint32_t *)(f->esp + word);

	f->eax = filesize(fd);
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
	// syscall 2
	check_address(f, 2);
	fd = (int)*(uint32_t *)(f->esp + word);
	length = (unsigned)*(uint32_t *)(f->esp + word * 2);
	
	seek(fd, length);
	break;
    case SYS_TELL:                   /* Report current position in a file. */
	// syscall 1
	check_address(f, 1);
	fd = (int)*(uint32_t *)(f->esp + word);

	f->eax = tell(fd);
	break;
    case SYS_CLOSE:                  /* Close a file. */
	// syscall 1
	check_address(f, 1);
	fd = (int)*(uint32_t *)(f->esp + word);

	close(fd);
	break;

    /* Project 3 and optionally project 4. */
    case SYS_MMAP:                   /* Map a file into memory. */
	break;
    case SYS_MUNMAP:                 /* Remove a memory mapping. */
	break;

    /* Project 4 only. */
    case SYS_CHDIR:                  /* Change the current directory. */
        check_address(f, 1);

        f->eax = chdir((const char*)*(uint32_t *)(f->esp + word));
	break;
    case SYS_MKDIR:                  /* Create a directory. */
        check_address(f, 1);
	fd = (int)*(uint32_t *)(f->esp + word);
      
        f->eax = mkdir((const char*)*(uint32_t *)(f->esp + word));
	break;
    case SYS_READDIR:                /* Reads a directory entry. */
        check_address(f, 2);
        
        fd = (int)*(uint32_t *)(f->esp + word);
        f->eax = readdir(fd, (const char*)*(uint32_t *)(f->esp + 2 * word));
	break;
    case SYS_ISDIR:                  /* Tests if a fd represents a directory. */
        check_address(f, 1);

        fd = (int)*(uint32_t *)(f->esp + word);
        f->eax = isdir(fd);
	break;
    case SYS_INUMBER:                /* Returns the inode number for a fd. */
        check_address(f, 1);

        fd = (int)*(uint32_t *)(f->esp + word); 
        f->eax = inumber(fd);
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

  thread_exit();
}

tid_t exec (const char *file){
  return process_execute(file);
}

int wait (tid_t pid){
  return process_wait(pid);
}

bool create (const char *file, unsigned initial_size){
  if(file == NULL)
    exit(-1);
  if (strlen(file) > NAME_MAX)
    return false; 
  
  return filesys_create(file, initial_size, false);  
}

bool remove (const char *file){
  /* directory file remove시 empty check 해야함 */
  return filesys_remove(file);
}

int open (const char *file){
  int ret = -1;

  if(file == NULL)
    return -1;
  
  struct thread *cur = thread_current();
  struct file *f = filesys_open(file);
  /* file이 존재하는 경우 */
  if(f != NULL){
    /* open directory 체크 */ 
    struct inode *disk_inode = file_get_inode (f);
    struct dir *dir;
    if (disk_inode != NULL && inode_is_dir (disk_inode))
      dir = dir_open (inode_reopen (disk_inode));
    else
      dir = NULL;
    
    /* file descriptor table 탐색하여
       가장 작은 free fd에 open한 file의 포인터 저장 */
    for(int i=2; i<128; i++){
      if(cur->file_desc[i]->f == NULL){
        cur->file_desc[i]->f = f;		// file 저장
        cur->file_desc[i]->d = dir;		// direcotry 저장
        ret = i;

	/* Denying Writes to Executable */
	struct thread *cur = thread_current();
        if(strcmp(cur->name, file) == 0)
	  file_deny_write(f);

	break;
      }
    }
  }

  return ret; 
}

int filesize (int fd){
  struct thread *cur = thread_current();

  if(fd < 0 || fd >= 128)
    exit(-1);

  if(cur->file_desc[fd]->f == NULL)
    exit(-1);

  return file_length(cur->file_desc[fd]->f);
}

int read (int fd, void *buffer, unsigned length){
  int ret = -1;

  if(fd < 0 || fd >= 128)
    exit(-1);

  if(is_kernel_vaddr(buffer))
    exit(-1);

  lock_acquire(&file_read_write);
  /* Critical Section */
  if(fd == STDIN_FILENO){
    unsigned read_bytes = 0;

    while(read_bytes < length){
      uint8_t key = input_getc();
      ((char *)buffer)[read_bytes++] = key;
      if(key == '\0')
        break;
      read_bytes++;      
    }

    ret = read_bytes;
  }
  else{
    struct thread *cur = thread_current();
    if(cur->file_desc[fd]->f != NULL)
      ret = file_read(cur->file_desc[fd]->f, buffer, length);
  }
  /* Critical Section */
  lock_release(&file_read_write);

  return ret;
}

int write (int fd, const void *buffer, unsigned length)
{
  int ret = -1;

  if(fd < 0 || fd >= 128)
    exit(-1);

  lock_acquire(&file_read_write);
  /* Critical Section */
  if(fd == STDOUT_FILENO){    
    putbuf(buffer, length);
    ret = length;
  }
  else if (!isdir (fd)){
    struct thread *cur = thread_current();
    if(cur->file_desc[fd]->f != NULL)
      ret = file_write(cur->file_desc[fd]->f, buffer, length);
  }
  /* Critical Section */
  lock_release(&file_read_write);

  return ret;
}

void seek (int fd, unsigned position){
  struct thread *cur = thread_current();

  /* file의 크기 이상으로 pos를 변경할 경우 error */
//  if(filesize(fd) <= position)
//    exit(-1);

  file_seek(cur->file_desc[fd]->f, position);	
}

unsigned tell (int fd){
  struct thread *cur = thread_current();

  return file_tell(cur->file_desc[fd]->f);
}

void close (int fd){
  struct thread *cur = thread_current();

  /* 0미만 128이상의 fd number를 close하는 경우 error
     STDIN(0), STDOUT(1) file descriptor를 close하는 경우 error */
  if(fd <= 1 || fd >= 128)
    exit(-1); 

  file_close(cur->file_desc[fd]->f);
  cur->file_desc[fd]->f = NULL;
  /* 각 fd에 해당하는 directory도 닫아줌, for persistence */  
  dir_close (cur->file_desc[fd]->d);
  cur->file_desc[fd]->d = NULL;	
}
/*
mapid_t mmap (int fd, void *addr){
	return -1;
}
void munmap (mapid_t){
} */
bool chdir (const char *dir){
  struct dir *directory = open_directory_path (dir);
  if (directory){
    /* working directory를 "dir" 경로로 변경 */
    dir_close (thread_current ()->cur_dir);
    thread_current ()->cur_dir = directory;
    return true;
  } 

  return false;
}

bool mkdir (const char *dir){
  /* directory로 file 생성 */
  return filesys_create(dir, 0, true);
}

bool readdir (int fd, char* name){
  struct file *file = thread_current ()->file_desc[fd]->f;  
 
  if(file){
    struct inode *disk_inode = file_get_inode (file); 
    /* 해당 directory의 entry 읽음 */
    if (disk_inode != NULL && inode_is_dir (disk_inode))
      return dir_readdir (thread_current ()->file_desc[fd]->d, name);
  }

//  return false;
}

bool isdir (int fd){
  struct file *file = thread_current ()->file_desc[fd]->f;
  /* 해당 fd file이 directory인지 여부 return */ 
  if(file)
    return inode_is_dir (file_get_inode (file));
  
  return false;
}

int inumber (int fd){
  struct file *file = thread_current ()->file_desc[fd]->f;
  /* 해당 fd file의 inode sector return */
  if(file) 
    return inode_get_inumber (file_get_inode (file));
  
  return -1;
}

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
