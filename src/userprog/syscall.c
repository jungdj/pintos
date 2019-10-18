#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include <devices/shutdown.h>
#include <filesys/filesys.h>
#include <filesys/file.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "pagedir.h"
#include "process.h"

static void syscall_handler (struct intr_frame *);
static void halt ();
static void exit (int status);
static int exec (const char *cmd_line);
static int wait (int pid);
static bool create (const char *filename, unsigned initial_size);
static bool remove (const char *file);
static int open (const char *file);
static int filesize (int fd);
static int read (int fd, void *buffer, unsigned length);
static int write (int fd, const void *buffer, unsigned size);
static void seek (int fd, unsigned position);
static unsigned tell (int fd);
static void close (int fd);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
privilege_check (void *uaddr)
{
  if (uaddr == NULL || !is_user_vaddr(uaddr))
  {
    exit(-1);
  }
  /* Check given pointer is mapped or unmapped */
  uint32_t *pd = thread_current()->pagedir;
  if (pagedir_get_page (pd, uaddr) == NULL)
  {
    exit (-1);
  }
}


static void *
read_argument (void **esp, void **args, uint8_t num)
{
  int i;
  void *tmp = *esp +4;

  for (i = 0; i < num; i++) {
    args[i] = tmp;
    tmp += 4;
  }
}

/* return file that match fd.
 * if there are no matched file, return NULL
 * */
static struct file *
find_file (int fd)
{
  struct thread *t = thread_current ();
  struct list_elem *e;
  struct file_descriptor *fd_info;
  struct file *file = NULL;

  for (e = list_begin (&t->fds); e != list_end (&t->fds); e = e->next)
  {
    fd_info = list_entry (e, struct file_descriptor, elem);
    if (fd_info->fd == fd) {
      file = fd_info->file;
      return file;
    }
  }

  return NULL;
}

static void
syscall_handler(struct intr_frame *f) {
  void *args[3];
  int syscall_number;
  privilege_check (f->esp);
  syscall_number = *(int *) (f->esp);

  switch (syscall_number) {
    case SYS_HALT:
//      printf ("syscall HALT called\n");
        halt ();
      break;
    case SYS_EXIT:
      read_argument (&(f->esp), args, 1);
      f->eax = *(int *) args[0];
      exit (*(int *) args[0]);
      break;
    case SYS_EXEC:
//      printf ("syscall EXEC called\n");
      read_argument (&(f->esp), args, 1);
      privilege_check (args[0]);
      f->eax = exec ((char *) *(uint32_t *) args[0]);
      break;
    case SYS_WAIT:
//      printf ("syscall WAIT called\n");
      read_argument (&(f->esp), args, 1);
      f->eax = wait (*(int *) args[0]);
      break;
    case SYS_CREATE:
//      printf ("syscall CREATE called\n");
      read_argument (&(f->esp), args, 2);
      privilege_check (args[0]);
//      printf("%s\n",(char *) *(uint32_t *) args[0]);
      f->eax = create ( (char *) *(uint32_t *) args[0], *(unsigned *) args[1]);
      break;
    case SYS_REMOVE:
//      printf ("syscall REMOVE called\n");

      break;
    case SYS_OPEN:
      read_argument (&(f->esp), args, 1);
      privilege_check (args[0]);
//      hex_dump((uintptr_t *) esp, esp, 0xc0000000 - (uint32_t) esp, true);
      f->eax = open ((char *) *(uint32_t *) args[0]);
      break;
    case SYS_FILESIZE:
      read_argument (&(f->esp), args, 1);
      f->eax = filesize(*(int *)args[0]);
//      printf ("syscall FILESIZE called\n");
      break;
    case SYS_READ:
//      printf ("syscall READ called\n");
      read_argument (&(f->esp), args, 3);
      privilege_check (args[1]);
      f->eax = read (*(int *) args[0], (void *) *(uint32_t *) args[1], *(unsigned *) args[2]);
      break;
    case SYS_WRITE:
      read_argument (&(f->esp), args, 3);
      privilege_check (args[1]);
      f->eax = write (*(int *) args[0], (void *) *(uint32_t *) args[1], *(unsigned *) args[2]);
      break;
    case SYS_SEEK:
//      printf ("syscall SEEK called\n");

      break;
    case SYS_TELL:
//      printf ("syscall TELL called\n");

      break;
    case SYS_CLOSE:
//      printf ("syscall CLOSE called\n");
      read_argument (&(f->esp), args, 1);
      privilege_check (args[1]);
      close (*(int *) args[0]);
      break;
      /* Project 3 and optionally project 4. */
    case SYS_MMAP:
//      printf ("syscall MMAP called\n");

      break;
    case SYS_MUNMAP:
//      printf ("syscall MUNMAP called\n");

      break;

      /* Project 4 only. */
    case SYS_CHDIR:

      break;
    case SYS_MKDIR:

      break;
    case SYS_READDIR:

      break;
    case SYS_ISDIR:

      break;
    case SYS_INUMBER:

      break;
    default:
//      printf("syscall default called\n");
      break;
  }
}

static void
halt ()
{
  shutdown_power_off();
}

static void
exit(int status)
{
  struct thread *t = thread_current ();

  t->exit_status = status;
  sema_up (&t->wait_sema);

  printf("%s: exit(%d)\n", t->name, status);
  thread_exit ();
}

static int
exec (const char *cmd_line)
{
  int tid;
  tid = process_execute(cmd_line);
  if (tid == TID_ERROR) {
    return -1;
  }
  return tid;
}

static int
wait (int pid)
{
  return process_wait (pid);
}

static bool create
(const char *filename, unsigned initial_size)
{
  return filesys_create (filename, initial_size);
}

static int
write (int fd, const void *buffer, unsigned size)
{
  if (fd == 1) {
    putbuf (buffer, size);
    return size;
  } else {

    return size;
  }
}

static int
open (const char *file_name)
{
  struct thread *t = thread_current ();
  struct file_descriptor fd;
  struct file *file = NULL;

  memset (&fd, 0, sizeof fd);

  file = filesys_open (file_name);
  if (file == NULL)
  {
    return -1;
  }

  fd.file = file;
  fd.fd = ++(t->cur_fd);

  list_push_back (&t->fds, &fd.elem);
  return fd.fd;
}

static int
read (int fd, void *buffer, unsigned length)
{
  uint32_t left;
  uint32_t real;
  struct file *file = find_file(fd);
  if (file != NULL){
    left = file_length(file) - file_tell(file);
    real = left < length ? left : length;
    return file_read (file, buffer, real);
  }
  return -1;
}

static int
filesize (int fd)
{
  struct file *file = find_file(fd);
  if (file != NULL){
    return file_length(file);
  }
  return -1;
}

static void
close (int fd)
{
  struct file *file = find_file(fd);
  file_close(file);
}