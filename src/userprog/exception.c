#include "userprog/exception.h"
#include <inttypes.h>
#include <stdio.h>
#include "userprog/gdt.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

#include "userprog/pagedir.h"
#include "vm/frame.h"
#include "vm/page.h"

/* Number of page faults processed. */
static long long page_fault_cnt;

static void kill (struct intr_frame *);
static void page_fault (struct intr_frame *);
bool is_stack_growth(struct intr_frame *f, void* fault_addr, void* esp);
/* Registers handlers for interrupts that can be caused by user
   programs.

   In a real Unix-like OS, most of these interrupts would be
   passed along to the user process in the form of signals, as
   described in [SV-386] 3-24 and 3-25, but we don't implement
   signals.  Instead, we'll make them simply kill the user
   process.

   Page faults are an exception.  Here they are treated the same
   way as other exceptions, but this will need to change to
   implement virtual memory.

   Refer to [IA32-v3a] section 5.15 "Exception and Interrupt
   Reference" for a description of each of these exceptions. */
void
exception_init (void) 
{
  /* These exceptions can be raised explicitly by a user program,
     e.g. via the INT, INT3, INTO, and BOUND instructions.  Thus,
     we set DPL==3, meaning that user programs are allowed to
     invoke them via these instructions. */
  intr_register_int (3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
  intr_register_int (4, 3, INTR_ON, kill, "#OF Overflow Exception");
  intr_register_int (5, 3, INTR_ON, kill,
                     "#BR BOUND Range Exceeded Exception");

  /* These exceptions have DPL==0, preventing user processes from
     invoking them via the INT instruction.  They can still be
     caused indirectly, e.g. #DE can be caused by dividing by
     0.  */
  intr_register_int (0, 0, INTR_ON, kill, "#DE Divide Error");
  intr_register_int (1, 0, INTR_ON, kill, "#DB Debug Exception");
  intr_register_int (6, 0, INTR_ON, kill, "#UD Invalid Opcode Exception");
  intr_register_int (7, 0, INTR_ON, kill,
                     "#NM Device Not Available Exception");
  intr_register_int (11, 0, INTR_ON, kill, "#NP Segment Not Present");
  intr_register_int (12, 0, INTR_ON, kill, "#SS Stack Fault Exception");
  intr_register_int (13, 0, INTR_ON, kill, "#GP General Protection Exception");
  intr_register_int (16, 0, INTR_ON, kill, "#MF x87 FPU Floating-Point Error");
  intr_register_int (19, 0, INTR_ON, kill,
                     "#XF SIMD Floating-Point Exception");

  /* Most exceptions can be handled with interrupts turned on.
     We need to disable interrupts for page faults because the
     fault address is stored in CR2 and needs to be preserved. */
  intr_register_int (14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");
}

/* Prints exception statistics. */
void
exception_print_stats (void) 
{
  printf ("Exception: %lld page faults\n", page_fault_cnt);
}

/* Handler for an exception (probably) caused by a user process. */
static void
kill (struct intr_frame *f) 
{
  /* This interrupt is one (probably) caused by a user process.
     For example, the process might have tried to access unmapped
     virtual memory (a page fault).  For now, we simply kill the
     user process.  Later, we'll want to handle page faults in
     the kernel.  Real Unix-like operating systems pass most
     exceptions back to the process via signals, but we don't
     implement them. */
     
  /* The interrupt frame's code segment value tells us where the
     exception originated. */
  switch (f->cs)
    {
    case SEL_UCSEG:
      /* User's code segment, so it's a user exception, as we
         expected.  Kill the user process.  */
      printf ("%s: dying due to interrupt %#04x (%s).\n",
              thread_name (), f->vec_no, intr_name (f->vec_no));
      intr_dump_frame (f);
      thread_exit (); 

    case SEL_KCSEG:
      /* Kernel's code segment, which indicates a kernel bug.
         Kernel code shouldn't throw exceptions.  (Page faults
         may cause kernel exceptions--but they shouldn't arrive
         here.)  Panic the kernel to make the point.  */
      intr_dump_frame (f);
      PANIC ("Kernel bug - unexpected interrupt in kernel"); 

    default:
      /* Some other code segment?  Shouldn't happen.  Panic the
         kernel. */
      printf ("Interrupt %#04x (%s) in unknown segment %04x\n",
             f->vec_no, intr_name (f->vec_no), f->cs);
      thread_exit ();
    }
}

/* Page fault handler.  This is a skeleton that must be filled in
   to implement virtual memory.  Some solutions to project 2 may
   also require modifying this code.

   At entry, the address that faulted is in CR2 (Control Register
   2) and information about the fault, formatted as described in
   the PF_* macros in exception.h, is in F's error_code member.  The
   example code here shows how to parse that information.  You
   can find more information about both of these in the
   description of "Interrupt 14--Page Fault Exception (#PF)" in
   [IA32-v3a] section 5.15 "Exception and Interrupt Reference". */
static void
page_fault (struct intr_frame *f) 
{
  bool not_present;  /* True: not-present page, false: writing r/o page. */
  bool write;        /* True: access was write, false: access was read. */
  bool user;         /* True: access by user, false: access by kernel. */
  void *fault_addr;  /* Fault address. */

   /* Obtain faulting address, the virtual address that was
     accessed to cause the fault.  It may point to code or to
     data.  It is not necessarily the address of the instruction
     that caused the fault (that's f->eip).
     See [IA32-v2a] "MOV--Move to/from Control Registers" and
     [IA32-v3a] 5.15 "Interrupt 14--Page Fault Exception
     (#PF)". */
   asm ("movl %%cr2, %0" : "=r" (fault_addr));

   /* Turn interrupts back on (they were only off so that we could
      be assured of reading CR2 before it changed). */
   intr_enable ();

   /* Count page faults. */
   page_fault_cnt++;

   /* Determine cause. */
   not_present = (f->error_code & PF_P) == 0;
   write = (f->error_code & PF_W) != 0;
   user = (f->error_code & PF_U) != 0;

   /* To implement virtual memory, delete the rest of the function
      body, and replace it with code that brings in the page to
      which fault_addr refers. */
   /*
   printf ("Page fault at %p: %s error %s page in %s context.\n",
            fault_addr,
            not_present ? "not present" : "rights violation",
            write ? "writing" : "reading",
            user ? "user" : "kernel");
   */

   struct thread * t = thread_current();
   void * esp = user ? f->esp : t->esp;
   /* 
   가장 기본 경우의 수 stack grow인지 아닌지
   터뜨려야하는 경우의 수
   1. 사용자 주소에서 데이터를 기대할 수 없을때
   2. 페이지가 커널 가상메모리 영역에 있을때, 
   3. 읽기전용에 쓰려고 할때 -> 아직 고려 못함 g d
   */
   
   void* fault_page = (void*) pg_round_down(fault_addr);
   //printf("excpetion start\n");
   

   /* valid는 아니지만 growable region이라면 */
   if(is_stack_growth(f, fault_addr, esp)){
      //printf("first if statement\n");
      // int add_pages = (LOADER_PHYS_BASE - (unsigned)fault_page)/PGSIZE;
      // for (int i=0; i<add_pages; i++){
      void * newpage = allocate_new_frame(0, fault_page);
      if (newpage == NULL){
         printf("newpage?? \n\n");
      }
      struct frame_entry * frame_entry = lookup_frame(newpage);
      if(frame_entry == NULL){
         printf("ah ha!\n\n");
      } 
      pagedir_set_page(t->pagedir, frame_entry->allocated_page, frame_entry->physical_memory, true);
      fault_page += PGSIZE; // for next page chagne esp
   // }
      /* not yet implemented */
      /*
         User stack에 추가
         하나 추가할때마다 f->esp = f->esp - PGSIZE 
         restart process
      */
      
   }

   /* valid region에 있다면 --> 정보가 없는 경우*/
   else if (fault_addr != NULL && is_user_vaddr(fault_addr) && not_present){
      printf("second if statement\n");
      /* get empty frame d c
         empty frame이 없다면 eviction해서 하나 받아옴
         
         swap page info frame from disk

         Modify page and swap management table

         restart process
      */
      struct sup_pagetable_entry * sup_entry = sup_lookup(fault_addr);

      if (sup_entry == NULL){
         printf("PANIC in first if statement\n");
         kill (f);
      }
      
      //있으면, faulted address를 위한 새로운 프레임을 할당받음
      //이 페이지가 없으면 eviction 시켜야 하는데 일단 죽임
      void* new_frame = allocate_new_frame(0, fault_addr);
      if (new_frame==NULL){
         kill(f);
      }

      /*
      new frame에 sup table 참조하여 정보 넣기
      suppage가 status에 따라서 긁어옴
      */
      switch(sup_entry->status){
         case ON_FRAME:
            //이런 케이스 없을 듯
            break;
         case SWAPPED:
            //swap disk 미구현
            break;
         case ON_DISK:
            break;
      }
      
      sup_entry -> status = true;
      sup_entry -> physical_memory = new_frame;
      pagedir_set_dirty(t->pagedir, new_frame, false);
      kill(f);
   }
 
   /* valid도 아니고 grow도 못한다면 죽어라 */
   else{
      //printf("kill statement\n"); 
      kill (f);
   }
}

bool  
is_stack_growth(struct intr_frame *f, void* fault_addr, void* esp){
   if(!is_user_vaddr(fault_addr) || (esp - fault_addr) > 32){
      kill(f);
   }
   if (LOADER_PHYS_BASE - (unsigned)fault_addr <= 8<<20){
      return true;
   }
   return false;
}