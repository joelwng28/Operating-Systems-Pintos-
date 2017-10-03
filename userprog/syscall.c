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

bool
create (const char *file_name, unsigned size)
{
  bool status;

  if (!is_valid_ptr (file_name))
    exit (-1);

  lock_acquire (&fs_lock);
  status = filesys_create(file_name, size);  
  lock_release (&fs_lock);
  return status;
}

bool 
remove (const char *file_name)
{
  bool status;
  if (!is_valid_ptr (file_name))
    exit (-1);

  lock_acquire (&fs_lock);  
  status = filesys_remove (file_name);
  lock_release (&fs_lock);
  return status;
}

int
open (const char *file_name)
{
  struct file *f;
  struct file_descriptor *fd;
  int status = -1;
  
  if (!is_valid_ptr (file_name))
    exit (-1);

  lock_acquire (&fs_lock); 
 
  f = filesys_open (file_name);
  if (f != NULL)
    {
      fd = calloc (1, sizeof *fd);
      fd->fd_num = allocate_fd ();
      fd->owner = thread_current ()->tid;
      fd->file_struct = f;
      list_push_back (&open_files, &fd->elem);
      status = fd->fd_num;
    }
  lock_release (&fs_lock);
  return status;
}

int
filesize (int fd)
{
  struct file_descriptor *fd_struct;
  int status = -1;
  lock_acquire (&fs_lock); 
  fd_struct = get_open_file (fd);
  if (fd_struct != NULL)
    status = file_length (fd_struct->file_struct);
  lock_release (&fs_lock);
  return status;
}

int
read (int fd, void *buffer, unsigned size)
{
  struct file_descriptor *fd_struct;
  int status = 0;
  struct thread *t = thread_current ();

  unsigned buffer_size = size;
  void * buffer_tmp = buffer;

  /* check the user memory pointing by buffer are valid */
  while (buffer_tmp != NULL)
    {
      if (!is_valid_uvaddr (buffer_tmp))
	exit (-1);

      if (pagedir_get_page (t->pagedir, buffer_tmp) == NULL)   
	{ 
	  struct suppl_pte *spte;
	  spte = get_suppl_pte (&t->suppl_page_table, 
				pg_round_down (buffer_tmp));
	  if (spte != NULL && !spte->is_loaded)
	    load_page (spte);
          else if (spte == NULL && buffer_tmp >= (esp - 32))
	    grow_stack (buffer_tmp);
	  else
	    exit (-1);
	}
      
      /* Advance */
      if (buffer_size == 0)
	{
	  /* terminate the checking loop */
	  buffer_tmp = NULL;
	}
      else if (buffer_size > PGSIZE)
	{
	  buffer_tmp += PGSIZE;
	  buffer_size -= PGSIZE;
	}
      else
	{
	  /* last loop */
	  buffer_tmp = buffer + size - 1;
	  buffer_size = 0;
	}
    }

  lock_acquire (&fs_lock);   
  if (fd == STDOUT_FILENO)
      status = -1;
  else if (fd == STDIN_FILENO)
    {
      uint8_t c;
      unsigned counter = size;
      uint8_t *buf = buffer;
      while (counter > 1 && (c = input_getc()) != 0)
        {
          *buf = c;
          buffer++;
          counter--; 
        }
      *buf = 0;
      status = size - counter;
    }
  else 
    {
      fd_struct = get_open_file (fd);
      if (fd_struct != NULL)
	status = file_read (fd_struct->file_struct, buffer, size);
    }
  lock_release (&fs_lock);
  return status;
}

int
write (int fd, const void *buffer, unsigned size)
{
  struct file_descriptor *fd_struct;  
  int status = 0;

  unsigned buffer_size = size;
  void *buffer_tmp = buffer;

  /* check the user memory pointing by buffer are valid */
  while (buffer_tmp != NULL)
    {
      if (!is_valid_ptr (buffer_tmp))
	exit (-1);
      
      /* Advance */ 
      if (buffer_size > PGSIZE)
	{
	  buffer_tmp += PGSIZE;
	  buffer_size -= PGSIZE;
	}
      else if (buffer_size == 0)
	{
	  /* terminate the checking loop */
	  buffer_tmp = NULL;
	}
      else
	{
	  /* last loop */
	  buffer_tmp = buffer + size - 1;
	  buffer_size = 0;
	}
    }

  lock_acquire (&fs_lock); 
  if (fd == STDIN_FILENO)
    {
      status = -1;
    }
  else if (fd == STDOUT_FILENO)
    {
      putbuf (buffer, size);;
      status = size;
    }
  else 
    {
      fd_struct = get_open_file (fd);
      if (fd_struct != NULL)
	status = file_write (fd_struct->file_struct, buffer, size);
    }
  lock_release (&fs_lock);

  return status;
}

void 
seek (int fd, unsigned position)
{
  struct file_descriptor *fd_struct;
  lock_acquire (&fs_lock); 
  fd_struct = get_open_file (fd);
  if (fd_struct != NULL)
    file_seek (fd_struct->file_struct, position);
  lock_release (&fs_lock);
  return ;
}

unsigned 
tell (int fd)
{
  struct file_descriptor *fd_struct;
  int status = 0;
  lock_acquire (&fs_lock); 
  fd_struct = get_open_file (fd);
  if (fd_struct != NULL)
    status = file_tell (fd_struct->file_struct);
  lock_release (&fs_lock);
  return status;
}

void 
close (int fd)
{
  struct file_descriptor *fd_struct;
  lock_acquire (&fs_lock); 
  fd_struct = get_open_file (fd);
  if (fd_struct != NULL && fd_struct->owner == thread_current ()->tid)
    close_open_file (fd);
  lock_release (&fs_lock);
  return ; 
}

int*
pop(int *ptr){
	if(!is_user_vaddr(ptr)){
		exit(-1);
	}
	return ptr + 1;
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
	int *esp = f->esp;
	int syscall_number = *esp;
	if(!is_user_vaddr(syscall_number)){
		exit(-1);
	}
	switch (syscall_number)
        {
        case SYS_HALT:
          	shutdown_power_off ();
          	break;
        case SYS_EXIT:
          	exit (*pop(esp));
          	break;
         //process.c
        case SYS_EXEC:
          	f->eax = exec ((char *) *pop(esp));
          	break;
         //process.c
        case SYS_WAIT:
          	f->eax = process_wait(*pop(esp));
          	break;
        case SYS_CREATE:
          	f->eax = create ((char *) *pop(esp), *pop(esp);
          	break;
        case SYS_REMOVE:
          	f->eax = remove ((char *) *pop(esp));
          	break;
        case SYS_OPEN:
          	f->eax = open ((char *) *pop(esp));
          	break;
        case SYS_FILESIZE:
	  		f->eax = filesize (*pop(esp));
	  		break;
        case SYS_READ:
          	f->eax = read (*pop(esp), (void *) *pop(esp), *pop(esp));
          	break;
        case SYS_WRITE:
          	f->eax = write (*pop(esp), (void *) *pop(esp), *pop(esp));
          	break;
        case SYS_SEEK:
          	seek (*pop(esp), *pop(esp));
          	break;
        case SYS_TELL:
          	f->eax = tell (*pop(esp));
          	break;
        case SYS_CLOSE:
          	close (*pop(esp));
          	break;
        default:
          	break;
      	}	
	}
}
