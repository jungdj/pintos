#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include <devices/shutdown.h>
#include <filesys/filesys.h>
#include <filesys/file.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "pagedir.h"
#include "process.h"

static void syscall_handler (struct intr_frame *);
static void halt (void);
static void exit (int status);
static int exec (const char *cmd_line);
static int wait (int pid);
static bool create (const char *filename, unsigned initial_size);
//static bool remove (const char *file);
static int open (const char *file);
static int filesize (int fd);
static int read (int fd, void *buffer, unsigned length);
static int write (int fd, const void *buffer, unsigned size);
static void seek (int fd, unsigned position);
//static unsigned tell (int fd);
static void close (int fd);

struct semaphore filesys_sema;

void sema_up_filesys ()
{
  sema_up (&filesys_sema);
}

void sema_down_filesys ()
{
  sema_down (&filesys_sema);
}

void
syscall_init (void) 
{
  sema_init (&filesys_sema, 1);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
is_valid_uaddr (void *uaddr)
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

static void
is_valid_arg (void *arg, size_t size)
{
  if (arg == NULL)
  {
    exit (-1);
  }
  void* uaddr = (void *) *(uint32_t *) arg;
  is_valid_uaddr (uaddr);
  is_valid_uaddr (uaddr + size);
}

static void
read_argument (void **esp, void **args, uint8_t num)
{
  int i;
  void *tmp = *esp +4;

  for (i = 0; i < num; i++) {
    args[i] = tmp;
    tmp += 4;
  }
  is_valid_uaddr (tmp);
}

static void
syscall_handler(struct intr_frame *f) {
  void *args[3];
  int syscall_number;
  is_valid_uaddr (f->esp);
  is_valid_uaddr (f->esp + sizeof (int));
  syscall_number = *(int *) (f->esp);
//  printf("Syscall nunmber %d\n", syscall_number);

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
      is_valid_arg(args[0], sizeof (char *));
      f->eax = exec ((char *) *(uint32_t *) args[0]);
      break;
    case SYS_WAIT:
//      printf ("syscall WAIT called\n");
      read_argument (&(f->esp), args, 1);
      is_valid_uaddr (args[0] + 4);
      f->eax = wait (*(int *) args[0]);
      break;
    case SYS_CREATE:
//      printf ("syscall CREATE called\n");
      read_argument (&(f->esp), args, 2);
      is_valid_arg(args[0], sizeof (char *));
      f->eax = create ( (char *) *(uint32_t *) args[0], *(unsigned *) args[1]);
      break;
    case SYS_REMOVE:
//      printf ("syscall REMOVE called\n");

      break;
    case SYS_OPEN:
      read_argument (&(f->esp), args, 1);
      is_valid_arg(args[0], sizeof (char *));
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
      is_valid_arg(args[1], sizeof (void *));
      f->eax = read (*(int *) args[0], (void *) *(uint32_t *) args[1], *(unsigned *) args[2]);
      break;
    case SYS_WRITE:
      read_argument (&(f->esp), args, 3);
      is_valid_arg(args[1], sizeof (void *));
      f->eax = write (*(int *) args[0], (void *) *(uint32_t *) args[1], *(unsigned *) args[2]);
      break;
    case SYS_SEEK:
//      printf ("syscall SEEK called\n");
      read_argument (&(f->esp), args, 2);
      seek (*(int *) args[0], *(unsigned *) args[1]);
      break;
    case SYS_TELL:
//      printf ("syscall TELL called\n");

      break;
    case SYS_CLOSE:
//      printf ("syscall CLOSE called\n");
      read_argument (&(f->esp), args, 1);
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
  shutdown_power_off ();
}

static void
exit(int status)
{
  struct thread *t = thread_current ();

  t->exit_status = status;

  thread_exit ();
}

static int
exec (const char *cmd_line)
{
  int tid;
  tid = process_execute (cmd_line);
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
  bool result;
  sema_down (&filesys_sema);
  result = filesys_create (filename, initial_size);
  sema_up (&filesys_sema);
  return result;
}

static int
write (int fd, const void *buffer, unsigned size)
{
  if (fd == 1) {
    putbuf (buffer, size);
    return size;
  } else {
    uint32_t left;
    uint32_t real;
    int result = -1;
    struct file *file;

    sema_down (&filesys_sema);
    file = find_file (fd);

    if (file != NULL){
      left = file_length (file) - file_tell (file);
      real = left < size ? left : size;
      result = file_write (file, buffer, real);
    }
    sema_up (&filesys_sema);

    return result;
  }
}

static int
open (const char *file_name)
{
  struct thread *t = thread_current ();
  struct file_descriptor *fd;
  struct file *file = NULL;
  int result = -1;

  fd = (struct file_descriptor *) malloc (sizeof (struct file_descriptor));
  memset (fd, 0, sizeof (struct file_descriptor));

  sema_down (&filesys_sema);

  file = filesys_open (file_name);
  if (file != NULL)
  {
    fd->file = file;
    fd->fd = ++(t->cur_fd);

    list_push_back (&t->fds, &fd->elem);
    result = fd->fd;
  }

  sema_up (&filesys_sema);
  return result;
}

static int
read (int fd, void *buffer, unsigned length)
{
  uint32_t left;
  uint32_t real;
  int result = -1;
  struct file *file;
  sema_down (&filesys_sema);
  file = find_file(fd);

  if (file != NULL){
    left = file_length(file) - file_tell(file);
    real = left < length ? left : length;
    result = file_read (file, buffer, real);
  }

  sema_up (&filesys_sema);
  return result;
}

static void
seek (int fd, unsigned position)
{
  sema_down (&filesys_sema);
  struct file *file = find_file (fd);

  if (file != NULL) {
    file_seek (file, position);
  }
  sema_up (&filesys_sema);
}

static int
filesize (int fd)
{
  int result = -1;
  sema_down (&filesys_sema);
  struct file *file = find_file (fd);

  if (file != NULL){
    result = file_length(file);
  }
  sema_up (&filesys_sema);

  return result;
}

static void
close (int fd)
{
  sema_down (&filesys_sema);
  struct file_descriptor *fd_info = find_fd (fd);
  if (fd_info != NULL) {
    list_remove (&fd_info->elem);
    file_close (fd_info->file);
    free (fd_info);
  }
  sema_up (&filesys_sema);
}