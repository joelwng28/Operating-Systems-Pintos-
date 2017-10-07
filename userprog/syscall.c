#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include <stdbool.h>
#include "filesys/filesys.h"

typedef int pid_t;

//for peek at parts of stack
#define syscallSize 1
#define intSize 1 
#define ptrSize 1

#define STDIN_FILENO 0
#define STDOUT_FILENO 1

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

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
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
    struct thread* curr = thread_current();
    printf("%s: exit(%d)\n", thread_name(), status);
    thread_exit();
}

/*Runs the executable whose name is given in cmd_line, passing any given arguments, 
and returns the new process's program id (pid). Must return pid -1, 
which otherwise should not be a valid pid, if the program cannot load or run for any reason. 
Thus, the parent process cannot return from the exec
until it knows whether the child process successfully loaded its executable.
You must use appropriate synchronization to ensure this.*/
static pid_t
exec(const char *cmd_line){ //todo: add synchronization for parent/child
  return process_execute(cmd_line);
}

/*Waits for a child process pid and retrieves the child's exit status.*/
static int
wait(pid_t pid){
  //todo: return status
  process_wait();
}

/*Creates a new file called file initially initial_size bytes in size.
 Returns true if successful, false otherwise. Creating a new file does not open it:
 opening the new file is a separate operation which would require a open system call.*/
static bool 
create (const char *file, unsigned initial_size){
 if(filesys_create(file, initial_size)){ //todo: synch
      return true;
  }
  return false;
}

/*Deletes the file called file. Returns true if successful, false otherwise.
A file may be removed regardless of whether it is open or closed,
and removing an open file does not close it. See Removing an Open File, for details.*/
static bool
remove (const char *file){
   if(filesys_remove(file)){
    return true;
   }
   return false;
}

/*Opens the file called file. Returns a nonnegative integer handle 
called a "file descriptor" (fd), or -1 if the file could not be opened.*/
static int
open (const char *file){ //implement FD
  return filesys_open(file);
}

/*Reads size bytes from the file open as fd into buffer.
 Returns the number of bytes actually read (0 at end of file), 
 or -1 if the file could not be read (due to a condition other than end of file).
 Fd 0 reads from the keyboard using input_getc().*/
static int
read (int fd, void *buffer, unsigned length){
  if(fd == STDIN_FILENO){
      uint8_t* local_buffer = (uint8_t *) buffer;
      for (int i = 0; i < length; i++){
        local_buffer[i] = input_getc();
      }
      return size;
  }
  //file_read()
}

/*Writes size bytes from buffer to the open file fd.
  Returns the number of bytes actually written, 
  which may be less than size if some bytes could not be written.*/
static int
write (int fd, const void *buffer, unsigned length){
  if(fd == STDOUT_FILENO){
    putbuf(buffer, length);
    return length; //no EOF for console
  }
  //todo: implement FDT and write to it
  //use file_write() to write to file
}

/*Changes the next byte to be read or written in open file fd to position,
  expressed in bytes from the beginning of the file. 
  (Thus, a position of 0 is the file's start.)*/
static void
seek (int fd, unsigned position){

}
 
 /*Returns the position of the next byte to be read or written in open file fd,
  expressed in bytes from the beginning of the file.*/
static unsigned 
tell (int fd){
  
}

/*Closes file descriptor fd.
 Exiting or terminating a process implicitly closes all its open file descriptors,
  as if by calling this function for each one.*/
static void 
close (int fd){

}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
  printf ("system call!\n");

  //peek at system call args from stack -> call corresponding handler
  int *esp = f->esp;
  int syscall_number = *esp;
  if(!is_user_vaddr(syscall_number)){
    exit(-1);
  }

  void *arg1, *arg2, *arg3;
  switch (syscall_number){
      case SYS_HALT:
          halt();
          break;
      case SYS_EXIT:
          arg1 = esp + syscallSize;
          if(isAddressValid(arg1)){
            f->error_code = *arg1; //return to kernel?
            exit(*arg1);
          }
          break;
      case SYS_EXEC:
          arg1 = esp + syscallSize;
          if(isAddressValid(arg1)){ f->eax = exec(*arg1); }
    	    break;
      case SYS_WAIT:
          arg1 = esp + syscallSize;
          if(isAddressValid(arg1)){ wait(*arg1); }
          break;
      case SYS_CREATE:
          arg1 = esp + syscallSize;
          arg2 = esp + syscallSize + ptrSize;
          if(isAddressValid(arg1) && isAddressValid(arg2)){ f->eax = create(*arg1, *arg2); }
          break;
      case SYS_REMOVE:
          arg1 = esp + syscallSize;
          if(isAddressValid(arg1)){ f->eax = remove(*arg1); }
          break;
      case SYS_OPEN:
          arg1 = esp + syscallSize;
          if(isAddressValid(arg1)){ f->eax = open(*arg1); }
          break;
      case SYS_FILESIZE:
          //filesize(*(esp + syscallSize));
          break;
      case SYS_READ: 
          arg1 = esp + syscallSize;
          arg2 = esp + syscallSize + intSize;
          arg3 = esp + syscallSize + intSize + ptrSize;
          if(isAddressValid){ read(*arg1, *arg2, *arg3); }
          break;
      case SYS_WRITE: 
          arg1 = esp + syscallSize;
          arg2 = esp + syscallSize + intSize;
          arg3 = esp + syscallSize + intSize + ptrSize;
          if(isAddressValid){ write(*arg1, *arg2, *arg3); }
          break;
      case SYS_SEEK:
          //seek(*(esp + syscallSize), *(esp + syscallSize + intSize));
          break;
      case SYS_TELL:
          //tell(*(esp + syscallSize));
          break;
      case SYS_CLOSE:
          //close(*(esp + syscallSize));
          break;
      default:
          break;
  }

  thread_exit ();
}
