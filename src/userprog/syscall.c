#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include <devices/shutdown.h>
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "filesys/inode.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "pagedir.h"
#include "process.h"

#include "vm/page.h"

static void syscall_handler (struct intr_frame *);
static void halt (void);
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
static mapid_t mmap (int fd, void* upage);
static void munmap(mapid_t mapid);
static void check_pd (const void *uaddr);
void load_and_pin_buffer (const void *buffer, unsigned length);
void unpin_buffer (const void *buffer, unsigned length);
static bool chdir (const char *path);
static bool mkdir (const char *path);
static bool readdir (int fd, char *name);
static bool isdir (int fd);
static int inumber(int fd);

struct semaphore filesys_sema;

int global_mapid=1;

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
#ifndef VM
  /* Check given pointer is mapped or unmapped */
  uint32_t *pd = thread_current()->pagedir;
  if (pagedir_get_page (pd, uaddr) == NULL)
  {
    exit (-1);
  }
#endif
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
  thread_current ()->esp = f->esp;

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
      read_argument (&(f->esp), args, 1);
      is_valid_arg(args[0], sizeof (char *));
      f->eax = remove ((char *) *(uint32_t *) args[0]);
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
      read_argument (&(f->esp), args, 1);
      f->eax = tell (*(int *) args[0]);
      break;
    case SYS_CLOSE:
//      printf ("syscall CLOSE called\n");
      read_argument (&(f->esp), args, 1);
      close (*(int *) args[0]);
      break;
      /* Project 3 and optionally project 4. */
    case SYS_MMAP:
//      printf ("syscall MMAP called\n");
      read_argument (&(f->esp), args, 2);
      //is_valid_arg(args[1], sizeof (void *)); why???
      f->eax = mmap (*(int*)args[0], (void*) *(uint32_t *) args[1]);
      break;
    case SYS_MUNMAP:
      read_argument (&(f->esp), args, 1);
      munmap(*(mapid_t*) args[0]);
      break;

      /* Project 4 only. */
    case SYS_CHDIR:
      read_argument (&(f->esp), args, 1);
      is_valid_arg(args[0], sizeof (void *));
      f->eax = chdir (*(char **) args[0]);
      break;
    case SYS_MKDIR:
      read_argument (&(f->esp), args, 1);
      is_valid_arg(args[0], sizeof (void *));
      f->eax = mkdir (*(char **) args[0]);
      break;
    case SYS_READDIR:
      read_argument (&(f->esp), args, 2);
      is_valid_arg(args[1], sizeof (void *));
      f->eax = readdir (*(int *) args[0], *(char **) args[1]);
      break;
    case SYS_ISDIR:
      read_argument (&(f->esp), args, 1);
      f->eax = isdir (*(int *) args[0]);
      break;
    case SYS_INUMBER:
      read_argument (&(f->esp), args, 1);
      f->eax = inumber (*(int *) args[0]);
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

void
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
  sema_down (&filesys_sema);
  tid = process_execute (cmd_line);
  sema_up (&filesys_sema);
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
  result = filesys_create (filename, initial_size, false);
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
    int result = -1;
    struct file *file;

    sema_down (&filesys_sema);
    file = find_file (fd);
    struct inode * inode = file_get_inode (file);
    bool is_dir = inode_is_dir (inode);
    if (file != NULL && !is_dir){
#ifdef VM
      check_pd (buffer);
      load_and_pin_buffer (buffer, size);
#endif
      result = file_write (file, buffer, size);
#ifdef VM
      unpin_buffer (buffer, size);
#endif
    }
    sema_up (&filesys_sema);

    return result;
  }
}

static bool
remove(const char *file_name)
{
  bool success;
  sema_down(&filesys_sema);
  success = filesys_remove(file_name);
  sema_up(&filesys_sema);
  return success;
}

static int
open (const char *file_name)
{
  struct thread *t = thread_current ();
  struct file_descriptor *fd;
  struct file *file = NULL;
  int result = -1;
  struct inode *inode;
  struct dir *dir;

  fd = (struct file_descriptor *) malloc (sizeof (struct file_descriptor));
  memset (fd, 0, sizeof (struct file_descriptor));

  sema_down (&filesys_sema);

  file = filesys_open (file_name);
  if (file != NULL)
  {
    inode = file_get_inode (file);
    if (inode_is_dir (inode)) {
      dir = dir_open (inode);
      fd->dir = dir;
    }

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
  int result = -1;
  struct file *file;
  char tmp;
  sema_down (&filesys_sema);
  file = find_file(fd);
  struct inode * inode = file_get_inode (file);
  bool is_dir = inode_is_dir (inode);
  if (file != NULL && !is_dir){
#ifdef VM
    tmp = *(char *)buffer;
    if (tmp)
    {
      load_and_pin_buffer (buffer, length);
    }
    else {
      load_and_pin_buffer (buffer, length);
    }
#endif
    result = file_read (file, buffer, length);
#ifdef VM
    unpin_buffer (buffer, length);
#endif
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

static unsigned 
tell (int fd)
{
  int result;
  sema_down (&filesys_sema);
  struct file_descriptor *fd_info = find_fd (fd);
  result = file_tell(fd_info->file);
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
    if (fd_info->dir) {
      dir_close (fd_info->dir);
    }
    free (fd_info);
  }
  sema_up (&filesys_sema);
}

static mapid_t 
mmap (int fd, void* upage)
{
  sema_down(&filesys_sema);
  if (fd == 0 || fd == 1){
    goto fail;
  }
  if (upage == NULL || pg_ofs(upage) != 0){ 
    goto fail;
  }
  
  struct file * pre_file = find_file(fd);
  if (pre_file == NULL) goto fail;
  struct file * new_file = file_reopen(pre_file);
  int file_size = file_length(new_file);
  if (file_size == 0) goto fail;

  //check sup page entry
  struct thread *t = thread_current();
  for(int i=0; i<file_size; i=i+PGSIZE){
    if(sup_page_table_has_entry(t->spt, (upage+i))) goto fail;
  }
  
  //sup page make 이제부터 fail case가 없음
  for(int offset=0; offset<file_size; offset=offset+PGSIZE){
    size_t page_read_bytes = file_size-offset>PGSIZE ? PGSIZE : file_size-offset;
    size_t page_zero_bytes = PGSIZE-page_read_bytes;
    sup_page_reserve_segment(upage+offset, new_file, offset, page_read_bytes, page_zero_bytes, true);
  }
  
  //check finish. make map_desc
  struct map_desc * mdesc = malloc(sizeof (struct map_desc));
  mdesc->id = global_mapid++;
  mdesc->address = upage;
  mdesc->file = new_file;
  mdesc->size = file_size;
  list_push_back(&t->map_list, &mdesc->elem);

  sema_up(&filesys_sema);
  return mdesc->id;

  fail:
    sema_up(&filesys_sema);
    return -1;
}
static void
munmap(mapid_t mapid)
{
  sema_down(&filesys_sema);
  free_mmap_one(mapid);
  sema_up(&filesys_sema);
}

static void check_pd (const void *uaddr)
{
  void *upage;
  uint32_t *pd = thread_current()->pagedir;
  upage = pg_round_down (uaddr);
  if (pagedir_get_page (pd, upage) == NULL)
  {
    exit (-1);
  }
}

void
load_and_pin_buffer (const void *buffer, unsigned length)
{
  void *upage;
  for(upage = pg_round_down (buffer); upage < buffer + length; upage += PGSIZE)
  {
    sup_page_load_page_and_pin (upage, true, true);
  }
}

void
unpin_buffer (const void *buffer, unsigned length)
{
  void *upage;
  for(upage = pg_round_down (buffer); upage < buffer + length; upage += PGSIZE)
  {
    sup_page_update_frame_pinned (upage, false);
  }
}

static bool chdir (const char *path)
{
  sema_down (&filesys_sema);
  struct dir* dir = dir_open_path(path);
  if (dir != NULL) {
    dir_close (thread_current()->cur_dir);
    thread_current()->cur_dir = dir;
    sema_up (&filesys_sema);
    return true;
  }
  sema_up (&filesys_sema);
  return false;
}

static bool mkdir (const char *path)
{
  bool result;
  sema_down (&filesys_sema);
  result = filesys_create (path, 0, true);
  sema_up (&filesys_sema);
  return result;
}

static bool readdir (int fd, char *name)
{
  struct file *file;
  struct inode *inode;
  bool is_dir;
  struct dir *dir;
  bool success = false;

  if (strlen(name) > NAME_MAX) {
    return false;
  }

  sema_down (&filesys_sema);

  dir = fd_open_dir (fd);

  if (dir != NULL){
    success = dir_readdir (dir, name);
  }

  sema_up (&filesys_sema);
  return success;
}

static bool isdir (int fd)
{
  struct file *file;
  struct inode *inode;
  bool result = false;

  sema_down (&filesys_sema);
  file = find_file (fd);

  if (file != NULL){
    inode = file_get_inode (file);
    result = inode_is_dir (inode);
  }

  sema_up (&filesys_sema);
  return result;
}

static int inumber(int fd)
{
  struct file *file;
  struct inode *inode;
  int result = 0;

  sema_down (&filesys_sema);
  file = find_file (fd);

  if (file != NULL){
    inode = file_get_inode (file);
    result = inode_get_inumber (inode);
  }

  sema_up (&filesys_sema);
  return result;
}
