/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "threads/mmu.h"
#include "vm/vm.h"
static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* mmap */
static struct lock vmfile_lock;
void file_write_back(struct page* page, void* addr); 

/* The initializer of file vm */
void
vm_file_init (void) {
	lock_init(&vmfile_lock);
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
	return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	if(page == NULL) {
		return false;
	}
	struct file_page *file_page UNUSED = &page->file;

	/* Set up container to pass information to the lazy_load_segment. */
	struct load_info* container = page->uninit.aux;
	struct file* file = container->file;
	off_t offset = container->ofs;
	size_t page_read_bytes = container->read_bytes;
	size_t page_zero_bytes = container->zero_bytes;

	file_seek(file, offset);

	if(file_read(file, kva, page_read_bytes) != (int)page_read_bytes) {
		return false;
	}

	memset(kva+page_read_bytes, 0, page_zero_bytes);

	return true;

}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
	if(file_page == NULL) {
		return NULL;
	}

	file_write_back(page, page->va);

	/* set present bit 0 */
	pml4_clear_page(thread_current()->pml4, page->va);

	return true;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;

	if(file_page == NULL) {
		return NULL;
	}

	file_write_back(page, page->va);

	/* set present bit 0 */
	pml4_clear_page(thread_current()->pml4, page->va);


}

/* Do the mmap */
// lazy loading :: similar with load segment 
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
	struct file* target_file = file_reopen(file);

	/* [DIFF with load_segment in process] : to return mapped va*/ 
	void *start_addr = addr;
	
	int total_page_count =
        length <= PGSIZE
            ? 1
            : (length % PGSIZE
                   ? length / PGSIZE + 1
                   : length / PGSIZE); 

	
	/* set read and zero bytes */
	size_t read_bytes, zero_bytes; 
	
	// Check given length, if file length is smaller than length, put entire.
	// If file length is bigger than length, load the size of length only.
	read_bytes = file_length(target_file) < length ? file_length(target_file) : length;
	// the remain bytes which will be written in the last page
	zero_bytes = PGSIZE - (read_bytes % PGSIZE);

	ASSERT((read_bytes + zero_bytes) % PGSIZE == 0);
    ASSERT(pg_ofs(addr) == 0);  
    ASSERT(offset % PGSIZE == 0); 

	while((read_bytes> 0 || zero_bytes> 0)) {
	
		/* 	   
		We will read PAGE_READ_BYTES bytes from FILE
	    and zero the final PAGE_ZERO_BYTES bytes.*/
		
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* Set up container to pass information to the lazy_load_segment. */
		struct load_info* container = (struct load_info*)malloc(sizeof(struct load_info));
		// insert the given arguments
		container->file = target_file;
		container->ofs = offset;
		container->read_bytes = page_read_bytes;
		container->zero_bytes = page_zero_bytes;
		container->writable = writable;

		
		/* [DIFF with load_segment in process] */ 
		if (!vm_alloc_page_with_initializer (VM_FILE, addr,
					writable, lazy_load_segment, container))
			{	
				
				return NULL;}
		
		struct page *p = spt_find_page(&thread_current()->spt, start_addr);
        p->mapped_page_count = total_page_count;
		
		/* Advance for moving next page */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		addr += PGSIZE;
		offset += page_read_bytes;
		
	}

	/* [DIFF with load_segment in process] */ 
	return start_addr;

}

/* Do the munmap */
void
do_munmap (void *addr) {

	while(true) {
		struct page *p = spt_find_page(&thread_current()->spt, addr);
		if(p==NULL){
			break;
		}
		file_write_back(p, addr);
		addr += PGSIZE;
	}

}


void
file_write_back(struct page* target_page, void* addr) {

	if(target_page == NULL) {
		return NULL;
	}

	if(target_page->uninit.type != VM_FILE) {
		return NULL;
	}

	struct load_info* container = target_page->uninit.aux;

	//check the dirty bit
	if(pml4_is_dirty(thread_current()->pml4, target_page->va)) {
		//write file in the disk
		lock_acquire(&vmfile_lock);
		file_write_at(container->file, addr, container->read_bytes, container->ofs);
		pml4_set_dirty(thread_current()->pml4, target_page->va, 0);
		lock_release(&vmfile_lock);
	}
}