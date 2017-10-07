#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include <stdbool.h>
#include "filesys/filesys.h"

// May need to remove; for process_execute
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "devices/shutdown.h"

typedef int pid_t;

static void syscall_handler (struct intr_frame *);

//from user/lib/syscall.h
// void halt (void) NO_RETURN;
// void exit (int status) NO_RETURN;
// pid_t exec (const char *file);
// int wait (pid_t);
// bool create (const char *file, unsigned initial_size);
// bool remove (const char *file);
// int open (const char *file);
// int filesize (int fd);
// int read (int fd, void *buffer, unsigned length);
// int write (int fd, const void *buffer, unsigned length);
// void seek (int fd, unsigned position);
// unsigned tell (int fd);
// void close (int fd);

// Declaring for use in exit
static bool isAddressValid(void *pointer);

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* Terminates Pintos by calling shutdown_power_off() 
(declared in threads/init.h). This should be seldom used, 
because you lose some information about possible deadlock situations, etc.*/
static void 
halt(void){ 
  shutdown_power_off();
}

/*Terminates the current user program, returning status to the kernel. 
If the process's parent waits for it (see below), this is the status 
that will be returned. Conventionally, a status of 0 indicates success 
and nonzero values indicate errors. */
static void 
exit(int status){ //todo: tell waiting parent, child exited
  //if(isAddressValid(&status)){
    /* Getting exit status from stack
    int *argPointer = esp;
    argPointer++;
    int status = *argPointer;
    */
    struct thread* curr = thread_current();
    printf ("%s: exit(%d)\n", thread_name(), status); 
    thread_exit();
  //}
}

//helper function to ensure pointer is in user space, not null, and mapped
static bool
isAddressValid(void *pointer){
  void *ptr = pagedir_get_page(thread_current()->pagedir, pointer);
  if(!is_user_vaddr(pointer) || (pointer == 0) || !ptr){
      exit(-1);
      return false;
  }
  return true;
}

/*Runs the executable whose name is given in cmd_line, passing any given arguments, 
and returns the new process's program id (pid). Must return pid -1, 
which otherwise should not be a valid pid, if the program cannot load or run for any reason. 
Thus, the parent process cannot return from the exec
until it knows whether the child process successfully loaded its executable.
You must use appropriate synchronization to ensure this.*/
static pid_t
exec(const char *cmd_line){ //todo: add synchronization for parent/child
  if(isAddressValid(cmd_line)){
   return process_execute(cmd_line);
 }
 return -1;
}

/*Waits for a child process pid and retrieves the child's exit status.*/
static int
wait(pid_t *pid){
  //todo: return status
   if(isAddressValid(pid)){
    return process_wait(*pid);
  }
  return -1;
}

/*Creates a new file called file initially initial_size bytes in size.
 Returns true if successful, false otherwise. Creating a new file does not open it:
 opening the new file is a separate operation which would require a open system call.*/
static bool 
create (const char *file, unsigned initial_size){
  //todo: synch
  //if(isAddressValid(&file) && isAddressValid(&initial_size)){
    if(filesys_create(file, initial_size)){
      return true;
    }
  //}
  return false;
}

/*Deletes the file called file. Returns true if successful, false otherwise.
A file may be removed regardless of whether it is open or closed,
and removing an open file does not close it. See Removing an Open File, for details.*/
static bool
remove (const char *file){
  if(isAddressValid(&file)){
     if(filesys_remove(file)){
      return true;
     }
   }
   return false;
}

/*Opens the file called file. Returns a nonnegative integer handle 
called a "file descriptor" (fd), or -1 if the file could not be opened.*/
static int
open (const char *file){
  if(isAddressValid(&file)){
    return filesys_open(file);
  }
  return -1;
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
  //peek at system call args from stack -> call corresponding handler
  int *esp = f->esp;
  int syscall_number = *esp;
  //hex_dump(esp, esp, 44, true);
  if(!is_user_vaddr(esp)){
    exit(-1);
  }

  switch (syscall_number){
      case SYS_HALT:
          halt();
          break;
      case SYS_EXIT:
          f->error_code = *(esp + 1); //return to kernel?
          exit(*(esp + 1));
          break;
      case SYS_EXEC:
      	  f->eax = exec(*(esp + 1));
    	  break;
      case SYS_WAIT:
          wait((esp + 1));
          break;
      case SYS_CREATE:
          f->eax = create(*(esp + 1), *(esp + 2));
          break;
      case SYS_REMOVE:
          f->eax = remove(*(esp + 1));
          break;
      case SYS_OPEN:
          f->eax = open(*(esp + 1));
          break;
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
      default:
          break;
  }
}
