#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

static void syscall_handler (struct intr_frame *);
static void exit (int status);
static int write (int fd, const void *buffer, unsigned size);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* Reads a byte at user virtual address UADDR.
   UADDR must be below PHYS_BASE.
   Returns the byte value if successful, -1 if a segfault
   occurred. */
static int
get_user (const uint8_t *uaddr)
{
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
  : "=&a" (result) : "m" (*uaddr));
  return result;
}

/* Writes BYTE to user address UDST.
   UDST must be below PHYS_BASE.
   Returns true if successful, false if a segfault occurred. */
static bool
put_user (uint8_t *udst, uint8_t byte)
{
  int error_code;
  asm ("movl $1f, %0; movb %b2, %1; 1:"
  : "=&a" (error_code), "=m" (*udst) : "q" (byte));
  return error_code != -1;
}


static void *
esp_pop (void **esp)
{
  void* val = *esp;
  return val;
}

static void *
read_argument (void **esp, void **args, uint8_t num)
{
  int i;
  void *tmp = *esp +4;

  for (i = 0; i < num; i++) {
    args[i] = tmp;
    memcpy (args[i], tmp, 4);
    if (!(
      args[i] != NULL &&
      args[i] < PHYS_BASE
    )) {
      exit (-1);
    }
    tmp += 4;
  }
}

static void
syscall_handler(struct intr_frame *f) {
  //printf("system call!\n");
  void *args[3];
  int syscall_number;
  void **esp = &(f->esp);

  syscall_number = *(int *) esp_pop(&(f->esp));

  switch (syscall_number) {
    case SYS_HALT:
//      printf ("syscall HALT called\n");
      break;
    case SYS_EXIT:
//      printf ("syscall EXIT called\n");
      read_argument (&(f->esp), args, 1);
      f->eax = *(int *) args[0];
      exit (*(int *) args[0]);
      break;
    case SYS_EXEC:
//      printf ("syscall EXEC called\n");
      break;
    case SYS_WAIT:

//      printf ("syscall WAIT called\n");
      break;
    case SYS_CREATE:

//      printf ("syscall CREATE called\n");
      break;
    case SYS_REMOVE:
//      printf ("syscall REMOVE called\n");

      break;
    case SYS_OPEN:
//      printf ("syscall OPEN called\n");

      break;
    case SYS_FILESIZE:
//      printf ("syscall FILESIZE called\n");

      break;
    case SYS_READ:
//      printf ("syscall READ called\n");

      break;
    case SYS_WRITE:
      //printf ("syscall WRITE called\n");
      read_argument (&(f->esp), args, 3);
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
exit(int status)
{
  struct thread *t = thread_current ();
  printf("%s: exit(%d)\n", t->name, status);
  thread_exit ();
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