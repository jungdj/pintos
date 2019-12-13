#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);
void sema_up_filesys (void);
void sema_down_filesys (void);
void exit (int status);

#endif /* userprog/syscall.h */
