#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include <user/syscall.h>

void syscall_init (void);
void sema_up_filesys (void);
void sema_down_filesys (void);
void exit (int status);
void munmap(mapid_t mapid);
#endif /* userprog/syscall.h */
