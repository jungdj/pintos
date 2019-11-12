cmake_minimum_required(VERSION 3.14)
project(pintos)

include_directories (.)
include_directories (devices)
include_directories (examples)
include_directories (filesys)
include_directories (lib)
include_directories (lib/kernel)
include_directories (lib/user)
include_directories (misc)
include_directories (tests)
include_directories (tests/filesys)
include_directories (tests/filesys/base)
include_directories (tests/filesys/extended)
include_directories (tests/internal)
include_directories (tests/threads)
include_directories (tests/userprog)
include_directories (tests/userprog/no-vm)
include_directories (tests/vm)
include_directories (threads)
include_directories (userprog)
include_directories (utils)
include_directories (vm)

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DUSERPROG")

set(SOURCE_FILES threads/thread.c
        threads/synch.c
        threads/interrupt.c
        threads/palloc.c
        threads/malloc.c
        threads/init.c
        tests/threads/alarm-wait.c
        devices/timer.c
        userprog/exception.c
        userprog/process.c
        userprog/syscall.c
        vm/frame.c
        vm/page.c
        vm/swap.c
        filesys/filesys.c
        lib/string.c
        lib/user/syscall.c
        lib/kernel/list.c)

add_executable(pintos ${SOURCE_FILES})
