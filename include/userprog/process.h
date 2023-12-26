#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H
#define MAX_ARGS 128

#include "threads/thread.h"
#include "filesys/off_t.h"

//project 3
struct container
{
    struct file *file;
    off_t offset;
    size_t page_read_bytes;
    size_t page_zero_bytes;
};


tid_t process_create_initd (const char *file_name);
tid_t process_fork (const char *name, struct intr_frame *if_);
int process_exec (void *f_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (struct thread *next);

//project 3
#ifdef VM

bool setup_stack (struct intr_frame *if_);
bool install_page (void *upage, void *kpage, bool writable);
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes, bool writable) ;
static bool lazy_load_segment (struct page *page, void *aux);


#endif


#endif /* userprog/process.h */
