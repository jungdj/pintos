#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/synch.h"
#include "filesys/directory.h"
#ifdef VM
#include "vm/page.h"
#endif

/* States in a thread's life cycle. */
enum thread_status
  {
    THREAD_RUNNING,     /* Running thread. */
    THREAD_READY,       /* Not running but ready to run. */
    THREAD_BLOCKED,     /* Waiting for an event to trigger. */
    THREAD_DYING        /* About to be destroyed. */
  };

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

struct file_descriptor {
    int fd;
    struct dir* dir;
    struct file *file;
    struct list_elem elem;
};

/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */
struct thread
  {
    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */
    enum thread_status status;          /* Thread state. */
    char name[16];                      /* Name (for debugging purposes). */
    uint8_t *stack;                     /* Saved stack pointer. */
    int priority;                       /* Priority. */
    int exit_status;
    int effective_priority;             /* Effective Priority . */
    struct list locks;                  /* List of locks */
    struct lock *lock_needed;            /* Lock failed to acquire */
    int64_t awake_from;                 /* Awake from . */
    struct list_elem allelem;           /* List element for all threads list. */

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */

    struct list fds;
    struct file *executable;
    uint64_t cur_fd;

#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /* Page directory. */
#endif

    uint8_t *esp;
#ifdef VM
    struct hash *spt;
    struct list map_list;
#endif
    struct dir* cur_dir;                    /* current directory */
    /* Owned by thread.c. */
    unsigned magic;                     /* Detects stack overflow. */
  };

typedef int mapid_t;
struct map_desc {
    mapid_t id; //id
    void * address; //address mapped
    struct file * file; //mapped file
    int size;
    struct list_elem elem;
};

struct pcb {
    int ppid;
    int pid;
    int exit_status;
    bool process_loaded;
    struct semaphore wait_sema;
    struct semaphore process_loaded_sema;
    struct list_elem elem;
};

#ifdef USERPROG
struct pcb* find_pcb (int pid);
struct pcb* find_child_pcb (int pid);
void pcb_set_parent (int pid);
bool pcb_loaded (int pid);
void pcb_update_loaded (void);
void pcb_update_status (int status);
void pcb_wait_sema_up (void);
void pcb_p_loaded_sema_up (void);
void free_pcb (int pid);
struct file * find_file (int fd);
struct file_descriptor * find_fd (int fd);
#endif
#ifdef VM
void free_mmap_all(void);
bool free_mmap_one(mapid_t mapid);
struct map_desc * find_map_desc (mapid_t fd);
#endif

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

struct thread* find_thread (int tid);
#ifdef FILESYS
struct dir * fd_open_dir (int fd);
#endif

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

//static bool sleep_less (const struct list_elem *a_, const struct list_elem *_b, void *aux UNUSED);
//static bool priority_less (const struct list_elem *a_, const struct list_elem *b_, void *aux UNUSED);
//static bool e_priority_less (const struct list_elem *a_, const struct list_elem *b_, void *aux UNUSED);

void thread_sleep (int64_t ticks);
void thread_wake (void);
void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func (struct thread *t, void *aux);
void thread_foreach (thread_action_func *, void *);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

#endif /* threads/thread.h */
