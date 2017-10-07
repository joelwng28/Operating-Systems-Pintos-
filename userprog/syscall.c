#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
  int *esp = f->esp;
  //hex_dump(esp, esp, 44, true);
  int syscall_number = *esp;

  switch (syscall_number) {

    case SYS_WRITE:                  /* Write to a file. */
    {
	int *argPointer = esp;

	argPointer++;
	int fd = *(argPointer++);
	int bufferPointer = *(argPointer++);
	unsigned size = *((unsigned*)argPointer);
    	putbuf(bufferPointer, size);
	f->eax = 1;
    	break;
    }
    case SYS_EXIT:
    {
	// Getting exit status from stack
	int *argPointer = esp;
	argPointer++;
	int status = *argPointer;

	// Getting dying thread's name
	struct thread *current = thread_current();	
	printf("%s: exit(%d)\n", thread_name(), status);

    	thread_exit();
    }
    default:
    	printf("system call!\n");
    	break;
  }
}
