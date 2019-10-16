#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

static void syscall_handler (struct intr_frame *);
static void exit (int status);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void *
esp_pop (void **esp)
{
  void* val = *esp;
  esp += sizeof (void *);
  return val;
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
put_user (uint8_t *udst, uint8_t byte) {
  int error_code;
  asm ("movl $1f, %0; movb %b2, %1; 1:"
  : "=&a" (error_code), "=m" (*udst) : "q" (byte));
  return error_code != -1;
}

static bool
privilege_check (void *arg1, void *arg2, void *arg3)
{
  return (
    arg1 != NULL &&
    arg2 != NULL &&
    arg3 != NULL &&
    arg1 < PHYS_BASE &&
    arg2 < PHYS_BASE &&
    arg3 < PHYS_BASE
  );
}

static void
syscall_handler(struct intr_frame *f) {
  printf("syscall handler %x\n");
  printf("system call!\n");
  void *arg1;
  void *arg2;
  void *arg3;
  int syscall_number;
  syscall_number = *(int *) esp_pop(&(f->esp));

  switch (syscall_number) {
    case SYS_HALT:
      break;
    case SYS_EXIT:
      arg1 = esp_pop(&(f->esp));
      exit (*(int *) arg1);
      break;
    case SYS_EXEC:
      break;
    case SYS_WAIT:
      arg1 = esp_pop(&(f->esp));
      break;
    case SYS_CREATE:
      arg1 = esp_pop(&(f->esp));
      break;
    case SYS_REMOVE:
      arg1 = esp_pop(&(f->esp));
      break;
    case SYS_OPEN:
      arg1 = esp_pop(&(f->esp));
      break;
    case SYS_FILESIZE:
      arg1 = esp_pop(&(f->esp));
      break;
    case SYS_READ:
      arg1 = esp_pop(&(f->esp));
      arg2 = esp_pop(&(f->esp));
      arg3 = esp_pop(&(f->esp));
      break;
    case SYS_WRITE:
      arg1 = esp_pop(&(f->esp));
      arg2 = esp_pop(&(f->esp));
      arg3 = esp_pop(&(f->esp));
      break;
    case SYS_SEEK:
      arg1 = esp_pop(&(f->esp));
      arg2 = esp_pop(&(f->esp));
      break;
    case SYS_TELL:
      arg1 = esp_pop(&(f->esp));
      break;
    case SYS_CLOSE:
      arg1 = esp_pop(&(f->esp));
      break;
      /* Project 3 and optionally project 4. */
    case SYS_MMAP:
      arg1 = esp_pop(&(f->esp));
      arg2 = esp_pop(&(f->esp));
      break;
    case SYS_MUNMAP:
      arg1 = esp_pop(&(f->esp));
      break;

      /* Project 4 only. */
    case SYS_CHDIR:
      arg1 = esp_pop(&(f->esp));
      break;
    case SYS_MKDIR:
      arg1 = esp_pop(&(f->esp));
      break;
    case SYS_READDIR:
      arg1 = esp_pop(&(f->esp));
      arg2 = esp_pop(&(f->esp));
      break;
    case SYS_ISDIR:
      arg1 = esp_pop(&(f->esp));
      break;
    case SYS_INUMBER:
      arg1 = esp_pop(&(f->esp));
      break;
    default:
      break;
  }
}

static void
exit(int status)
{

  thread_exit ();
}