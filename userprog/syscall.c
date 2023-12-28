#include <debug.h>
#include "userprog/process.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "vm/vm.h"


void syscall_entry (void);
void syscall_handler (struct intr_frame *);
struct file *get_file_from_fd_table (int fd);
struct lock file_lock;

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);

	lock_init(&file_lock);
}

void check_address(void *addr) {
	
	struct thread *t = thread_current();
	if(!is_user_vaddr(addr) || addr == NULL ) {
		//printf("[check addr] fail in 1 or 2\n");
		exit(-1);
	}

	#ifdef VM
	//printf("[check addr] start\n");
	if(spt_find_page(&t->spt, addr)) {
		//printf("[check addr] page is found in spt\n");
		struct page* p = spt_find_page(&t->spt, addr);
		enum vm_type page_type = page_get_type(p);
		//printf("[check addr] passed page type: %d\n",page_type);

		if(pml4_get_page(t->pml4 , addr)== NULL) {
			//printf("[check addr] but page is not found in pml4\n");
		} 
		//printf("[check addr] passed page addr: %p / frame kva: %p \n", p->va, p->frame->kva);

	}
	if(spt_find_page(&t->spt, addr) == NULL) {
		//printf("[check addr] fail in 3\n");
		exit(-1);
	}
	#else
	if(pml4_get_page(t->pml4 , addr)== NULL) {
		//printf("[check addr] fail in 3\n");
		exit(-1);
	}
	#endif
	// if (!is_user_vaddr(addr) || addr == NULL || pml4_get_page(t->pml4 , addr) == NULL) {
	// 	printf("[check addr] addr is failed!\n");
	// 	exit(-1);
	// }
}

/* Validate given buffer by page size*/
void validate_buffer(void* buffer, size_t size, bool is_writable) {
	//printf("[validate_buffer] buffer:%p, size:%d, is_writable:%d\n", buffer, size, is_writable);
	if (buffer == NULL) {
		//printf("[validate_buffer] buffer == NULL\n");
		exit(-1);
	}

	if (buffer<= USER_STACK && buffer>=thread_current()->intr_rsp) {
		//printf("[validate_buffer] case 2\n");
		return;
	}

	void* start_addr = pg_round_down(buffer);
	void* end_addr = pg_round_down(buffer+size);

	for (void* addr = end_addr; addr>=start_addr; addr-=PGSIZE) {
		//check address's function
		if(addr == NULL || is_kernel_vaddr(addr)) {
			//printf("[validate_buffer] case 3\n");
			exit(-1);
		}

		//check_address(addr);
		struct page* traget_page= spt_find_page(&thread_current()->spt, addr);
		//printf("[validate buffer] traget_page->writable: %d \n", traget_page->writable);

		if(traget_page == NULL) {
			//printf("[validate_buffer] case 4\n");
			exit(-1);
		}
	
		if(traget_page->writable == false && is_writable==true) {

			//printf("[validate_buffer] case 5\n");

			//printf("[validate buffer] traget_page->writable: %d \n", traget_page->writable);
			//printf("[validate buffer] is_writable: %d \n", is_writable);

			//printf("mmap clean out issue out\n");
			exit(-1);		
		}
	}
	}

int add_file_to_fd_table (struct file *file) {
	struct thread *t = thread_current();
	struct file **fdt = t->fd_table;
	int fd = t->fd_idx;
	while (t->fd_table[fd] != NULL) {
		if (fd >= FDCOUNT_LIMIT) {
			t->fd_idx = FDCOUNT_LIMIT;
			return -1;
		}
		fd++;
	}
	t->fd_idx = fd;
	fdt[fd] = file;
	return fd;
}

void halt(void) {
	power_off();
}

void exit (int status) {
	thread_current()->exit_status = status;
	printf("%s: exit(%d)\n", thread_name(), thread_current()->exit_status);
	thread_exit();
}

tid_t fork (const char *thread_name, int (*f)(int)) {
	check_address(thread_name);
	return process_fork(thread_name, f);
}

int exec (const char *file) {
	check_address(file);
    if (process_exec((void *) file) < 0) {
		exit(-1);
	}
}

int wait (tid_t tid) {
	return process_wait (tid);
}

bool create (const char *file, unsigned initial_size) {
	check_address(file);
	//validate_buffer(file, initial_size, true);
	lock_acquire(&file_lock);
	bool res = filesys_create(file, initial_size);
	lock_release(&file_lock);

	return res;
}

bool remove (const char *file) {
	check_address(file);
	return filesys_remove(file);
}

int open (const char *file) {
	//printf("[syscall open] start with :%p \n", file);
	check_address(file);
	//printf("[syscall open] addr check passed\n");
	lock_acquire(&file_lock);
	struct file *file_info = filesys_open(file);
	lock_release(&file_lock);
	if (file_info == NULL) {
		//printf("[syscall open] crushed, file_info:%d\n", file_info);
		return -1;
	}
	int fd = add_file_to_fd_table(file_info);
	if (fd == -1) {
		//printf("[syscall open] crushed, fd:%d\n", fd);
		file_close(file_info);
	}
	//printf("[syscall open] end with :%d\n", fd);
	return fd;
}

int filesize (int fd) {
	return file_length(get_file_from_fd_table(fd));
}

int read (int fd, void *buffer, unsigned length) {
	//printf("[syscall read] start with :%d, %p \n", fd, buffer);
	//check_address(buffer);
	validate_buffer(buffer, length, true);

	int bytesRead = 0;
	if (fd == 0) { 
		for (int i = 0; i < length; i++) {
			char c = input_getc();
			((char *)buffer)[i] = c;
			bytesRead++;

			if (c == '\n') break;
		}
	} else if (fd == 1) {
		return -1;
	} else {
		//printf("[syscall read] fd else start\n");
		struct file *f = get_file_from_fd_table(fd);
		//printf("[syscall read] f : %p\n", f);
		if (f == NULL) {
			//printf("[syscall read] f is NULL!\n");
			return -1; 
		}
		
		//printf("[syscall read] fd bf lock\n");
		lock_acquire(&file_lock);
		bytesRead = file_read(f, buffer, length);
		lock_release(&file_lock);
		//printf("[syscall read] fd else end\n");
	}
	return bytesRead;
}

struct file *get_file_from_fd_table (int fd) {
	struct thread *t = thread_current();
	if (fd < 0 || fd >= 128) {
		return NULL;
	}
	return t->fd_table[fd];
}

int write (int fd, const void *buffer, unsigned length) {
	//check_address(buffer);
	//printf("[syscall write] fd:%d\n", fd);
	validate_buffer(buffer, length, false);
	int bytesRead = 0;
	//printf("[syscall write] validate_buffer end\n");

	if (fd == 0) {
		//printf("[syscall write] fd = %d\n", fd);
		return -1;
	} else if (fd == 1) {
		putbuf(buffer, length);
		//printf("[syscall write] bytesRead:%d\n", length);
		return length;
	} else {
		struct file *f = get_file_from_fd_table(fd);
		if (f == NULL) {
			//printf("[syscall write] f = %p\n", f);
			return -1;
		}
		lock_acquire(&file_lock);
		bytesRead = file_write(f, buffer, length);
		lock_release(&file_lock);
		//printf("[syscall write] lock finished\n");
		
	}
	return bytesRead;
}

void seek (int fd, unsigned position) {
	struct file *f = get_file_from_fd_table(fd);
	if (f == NULL) {
		return;
	}
	file_seek(f, position);
}

unsigned tell (int fd) {
	struct file *f = get_file_from_fd_table(fd);
	if (f == NULL) {
		return -1;
	}
	return file_tell(f);
}

void close (int fd) {
	struct thread *t = thread_current();
	struct file **fdt = t->fd_table;
	if (fd < 0 || fd >= 128) {
		return;
	}
	if (fdt[fd] == NULL) {
		return;
	}
	file_close(fdt[fd]);
	fdt[fd] = NULL;
}

void *
mmap (void *addr, size_t length, int writable, int fd, off_t offset) {
	//printf("[mmap] addr:%p / length:%d / writable:%d / fd:%d /offset: %d \n", addr, length, writable, fd, offset);
	struct file* target_file = get_file_from_fd_table(fd);

	// the file descriptors representing console input and output are not mappable
	if (fd == 0 || fd == 1) {
		//printf("[mmap] fail case 6 \n");
		return NULL;
		//exit(-1);
	}

	if(target_file == NULL) {
		//printf("[mmap] fail case 1 \n");
		//file이 존재하지 않을 때 실패
		return NULL;
	}

	//printf("[mmap] file's address: %p / size: %d\n", target_file, file_length(target_file));

	/* check fail cases bf do_mmap*/
	//if the file opened as fd has a length of zero bytes.
	if(file_length(target_file)==0) {
		//printf("[mmap] fail case 2 \n");
		return NULL;
	}

	/* page-aligned issues */
	// if offset(the starting point of file) is not page-aligned
	if(offset % PGSIZE != 0) {
		//printf("[mmap] fail case 3 \n");
		return NULL;
	}
	//if the addr(the starting point of addr) is not page-aligned
	// if(((uint64_t)addr % PGSIZE != 0)) {
	// 	return NULL;
	// }
	if (pg_round_down(addr) != addr || is_kernel_vaddr(addr)){
		//printf("[mmap] fail case 4 \n");
		return NULL;
	}
        

	/* addr / length /fd issue */
	// if addr is 0, it must fail, because some Pintos code assumes virtual page 0 is not mapped.
	// Your mmap should also fail when length is zero
	if(addr == 0 || (long long)length <= 0) {
		//printf("[mmap] fail case 5 \n");
		return NULL;
	}
	
	/* vm overlapping issues */

	/* if the range of pages mapped overlaps any existing set of mapped pages */ 
	// overlaps pages mapped at executable load time
	if(spt_find_page(&thread_current()->spt, addr)) {
		//printf("[mmap] fail case 7 \n");
		return NULL;
	}

	//printf("[mmap] start do mmap \n");
	void* res = do_mmap(addr, length, writable, target_file, offset);

	if (res == NULL) {
		return NULL;
	}

	struct page* p = spt_find_page(&thread_current()->spt, res);
	//printf("[mmap] ending page found : %p\n", p);
	struct load_info* container = p->uninit.aux;
	//printf("[mmap] container file addr:%p, addr:%p, container->read_bytes: %d, container->ofs:%d \n", container->file, addr, container->read_bytes, container->ofs);
	

	return res;
	//return do_mmap(addr, length, writable, target_file, offset);

}

void
munmap (void *addr) {
	// //printf("[munmap] start at %p \n", addr);
	// check_address(addr);
	// // printf("[munmap] checkaddr passed \n");
	// // struct page* page = spt_find_page(&thread_current()->spt, addr);
	// // printf("[munmap] page found %p \n", page);
	// // if(page==NULL || page_get_type(page)!=VM_FILE) {
	// // 	return NULL;
	// // }

	// struct page* p = spt_find_page(&thread_current()->spt, addr);
	// //printf("[mmap] page found : %p\n", p);
	// struct load_info* container = p->uninit.aux;
	// //printf("[munmap] container file addr:%p, addr:%p, container->read_bytes: %d, container->ofs:%d \n", container->file, addr, container->read_bytes, container->ofs);

	//lock_acquire(&file_lock);
	do_munmap(addr);
	//lock_release(&file_lock); 	
	//printf("[munmap] end\n");

	// struct supplemental_page_table *spt = &thread_current()->spt;
    // struct page *p = spt_find_page(spt, addr);
    // int count = p->mapped_page_count;
    // for (int i = 0; i < count; i++) {
    //     if (p) destroy(p);

    //     addr += PGSIZE;
    //     p = spt_find_page(spt, addr);
    // }
}



/* The main system call interface */
void
syscall_handler (struct intr_frame *f) {
	//printf("[syscall number] %d\n", f->R.rax);
	#ifdef VM
	/* When switched into kernel mode, 
	   save stack pointer in thread'*/
	thread_current()->intr_rsp = f->rsp;
	//printf("[syscall checking] curr intr_rsp %p,\n",thread_current()->intr_rsp);
	#endif
	switch (f->R.rax) {
		case SYS_HALT:
			halt();
			break;
		case SYS_EXIT:
			exit(f->R.rdi);
			break;
		case SYS_FORK:
			f->R.rax = fork(f->R.rdi, f);
			break;
		case SYS_EXEC:
			if (exec(f->R.rdi) < 0) {
				exit(-1);
			}
			break;
		case SYS_WAIT:
			f->R.rax = wait(f->R.rdi);
			break;
		case SYS_CREATE:
			f->R.rax = create(f->R.rdi, f->R.rsi);
			//printf("[syscall] created file:%p \n", f->R.rax);
			break;
		case SYS_REMOVE:
			f->R.rax = remove(f->R.rdi);
			break;
		case SYS_OPEN:
			f->R.rax = open(f->R.rdi);
			//printf("[syscall switch] fd?:%d\n", f->R.rax);
			break;
		case SYS_FILESIZE:
			f->R.rax = filesize(f->R.rdi);
			break;
		case SYS_READ:
			f->R.rax = read(f->R.rdi, f->R.rsi, f->R.rdx);
			break;
		case SYS_WRITE:
			f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
			break;
		case SYS_SEEK:
			seek(f->R.rdi, f->R.rsi);
			break;
		case SYS_TELL:
			f->R.rax = tell(f->R.rdi);
			break;
		case SYS_CLOSE:
			close(f->R.rdi);
			break;
		/* project 3 : mmap and unmmap */
		case SYS_MMAP:
			{f->R.rax = mmap(f->R.rdi, f->R.rsi, f->R.rdx, f->R.r10, f->R.r8);
			break;}
		case SYS_MUNMAP:
			{void* res = f->R.rdi;
			munmap(f->R.rdi);
			break;}
		default:
			exit(-1);
	}
	//printf("[syscall] end \n");
}
