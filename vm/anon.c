/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"
/* swap in/out */
#include "threads/vaddr.h"
#include "bitmap.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* bitmap swap in/out */
struct bitmap *swap_table;
/* bitmap is an array of bits, 
   each of which can be true or false.*/

/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	/*
	pintOS uses: 1:1 - swap
	Each bit represents one bit in the bitmap.
	*/
	swap_disk = disk_get(1,1); 
	size_t swap_size = disk_size(swap_disk) / SECTOR_CNT;
	swap_table = bitmap_create(swap_size);
}

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {

	/* Set up the handler */
	if(page == NULL || kva == NULL) {
		return false;
	}

	// clean the memory by 0
	struct uninit_page* uninit_page = &page->uninit;
	memset(uninit_page, 0, sizeof(struct uninit_page));  

	page->operations = &anon_ops; //set operation as its type's operation
	
	// the anon page only exits in memory : it isn't mapped into any disk sector 
	struct anon_page *anon_page = &page->anon; 
	anon_page->swap_index = -1; 

	return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	//get 
	struct anon_page *anon_page = &page->anon;
	int swap_page_no = anon_page->swap_index;

	if(bitmap_test(swap_table, swap_page_no) == false) {
		return false;
	}
	for (int i = 0; i<SECTOR_CNT; i++) {
		disk_read(swap_disk, swap_page_no * SECTOR_CNT + i, kva + DISK_SECTOR_SIZE*i);
	}

	bitmap_set(swap_table, swap_page_no, false);

	return true;

}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	
	int swap_page_no = bitmap_scan(swap_table, 0, 1, false);

	if(swap_page_no == BITMAP_ERROR) {
		return false;
	}

	for (int i = 0; i< SECTOR_CNT; i++) {
		disk_write(swap_disk, swap_page_no * SECTOR_CNT + i, page->va + DISK_SECTOR_SIZE * i);
	}

	bitmap_set(swap_table, swap_page_no, true);
	pml4_clear_page(thread_current()->pml4, page->va);

	anon_page->swap_index = swap_page_no;

	return true;

}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	// do not use free to page(->frame) here
	return;
}
