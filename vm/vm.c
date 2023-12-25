/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
/* project 3 */
#include "lib/kernel/hash.h"
#include "threads/vaddr.h"
#include "userprog/process.h"

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* project 3 : helpers for hash */
unsigned page_hash (const struct hash_elem *p_, void *aux UNUSED); 
bool page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED); 
struct page *page_lookup (const void *va, struct supplemental_page_table *spt);
void page_free(struct hash_elem* e, void* aux);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)
	struct supplemental_page_table *spt = &thread_current ()->spt;
	/* Check wheter the upage is already occupied or not. */
	void* upage_va = upage;
	if (spt_find_page (spt, upage) == NULL) {
		/* Create the page, fetch the initialier according to the VM type*/
		//struct page* new_page = calloc(1, sizeof(struct page));
		struct page* new_page = malloc(sizeof(struct page));
		if(new_page == NULL){
			goto err;
		}
		 /* TODO: and then create "uninit" page struct by calling uninit_new.*/ 
		//struct uninit_page* new_uninit_page;
		uninit_new(new_page, upage_va, init, type, aux, NULL);

		 /* TODO: You should modify the field after calling the uninit_new. */
		switch(VM_TYPE(type)) {
			case(VM_ANON):
				uninit_new(new_page, upage_va, init, type, aux, anon_initializer);
				new_page->writable = writable;
				break;
			case(VM_FILE):
				uninit_new(new_page, upage_va, init, type, aux, file_backed_initializer);
				new_page->writable = writable;
				break;
			} 

		/* TODO: Insert the page into the spt. */
		if(!spt_insert_page(spt, new_page)) {
			free(new_page);
			free(aux);
			goto err;
		}
		return true;
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	//printf("[spt find page] start\n");
	struct page *page = NULL;
	/* TODO: Fill this function. */
	page = page_lookup(va, spt);

	return page;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	// hash_insert will return NULL if it is succees to insert new one
	return hash_insert(&spt->spt_hash, &page->hash_elem) == NULL;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/

static struct frame *
vm_get_frame (void) {
	/* TODO: Fill this function. */
	struct frame *frame = calloc(1, sizeof(struct frame));
	if(! frame) {
		PANIC("[1] todo (vm_get_frame / swap out)");
		return NULL;
	}
	void* kvaddr = palloc_get_page(PAL_USER | PAL_ZERO);

	frame->kva = kvaddr;
	frame->page = NULL;
	//frame->page = pml4_get_page(&thread_current()->pml4, kvaddr);

	// (4) 페이지 할당에 실패한 경우 지금은 스왑 아웃을 처리할 필요가 없습니다. 당분간은 이러한 경우를 PANIC("할 일")으로 표시하면 됩니다.

	if(frame->kva == NULL) {
		PANIC("[2] todo (vm_get_frame / swap out)");
		return NULL;
	}

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	// If addr is null
	if(addr == NULL) {
		return false;
	}
	// If address is in kernel 
	if(is_kernel_vaddr(addr)) {
		return false;
	}
	// if it is present but fault is occured
	if(!not_present){
		return false;
	}
	/* TODO: Your code goes here */
	if((page =spt_find_page(spt, addr)) == NULL) {
		//printf("[vm_try_handle_fault] page is NULL!\n");
		false;
	}
	return vm_do_claim_page (page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va UNUSED) {
	/* TODO: Fill this function */
	// (1) get roundown va
	// (2) get page from spt(va->spt->page)
	struct thread* curr = thread_current();
	struct page *page = spt_find_page(&curr->spt, va);

	if(page==NULL) {
		return false;
	}
		
	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	//printf("[vm_do claim] start\n");
	//printf("[vm_do_claim_pager] page va addr: %p\n",page->va);
	//printf("[vm_do_claim_pager] page type: %d\n",page_get_type(page));

	//(1) get the frame
	struct frame *frame = vm_get_frame ();

	if(frame == NULL) {
		//printf("[vm_do claim] frame ==NULL\n");
		return false;
	}

	/* Set links */
	frame->page = page;
	page->frame = frame;

	bool res = pml4_set_page(thread_current()->pml4, page->va, frame->kva, page->writable);
	//printf("[vm_do claim] res: %d\n", res);
	//printf("[vm_do claim] frame kva: %p\n", page->frame->kva);
	
	return swap_in (page, frame->kva);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	//printf("[supplemental_page_table_init] 실행\n");
	struct hash* target_ht = &spt->spt_hash;
	// (1) init hash table
	if(! hash_init(target_ht, page_hash, page_less, NULL)) {
		return NULL;
	}
	// (2) 
	//printf("[supplemental_page_table_init] end\n");
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
			struct hash_iterator i;
			hash_first (&i, &src->spt_hash);
			while (hash_next (&i))
			{
				struct page* page = hash_entry (hash_cur (&i), struct page, hash_elem);
				enum vm_type type = VM_TYPE(page->operations->type);
				void* va = page->va;
				switch(type) {
					//anon and file have same machanism
					case VM_ANON:
						if(!vm_alloc_page(VM_ANON, va, page->writable)) {
							printf("[supplemental_page_table_copy] anon alloc failed\n");
							goto err;
						}
						if(!vm_claim_page(va)) {
							printf("[supplemental_page_table_copy] anon vm_claim_page failed\n");
							goto err;
						}
						break;
					case VM_FILE:
						if(!vm_alloc_page(VM_FILE, va, page->writable)) {
							printf("[supplemental_page_table_copy] file alloc failed\n");
							goto err;
						}
						if(vm_claim_page(va) == false) {
							printf("[supplemental_page_table_copy] file vm_claim_page failed\n");
							goto err;
						}
						break;
					//uninit page
					case VM_UNINIT:
						{//aux 
						void* aux = malloc(sizeof(struct load_info));
						if(aux == NULL) {
							printf("[supplemental_page_table_copy] uninit aux malloc is failed\n");
							goto err;
						}
						memcpy(aux, page->uninit.aux,sizeof(struct load_info));
						//page alloc
						if(vm_alloc_page_with_initializer(page->uninit.type, va, page->writable, page->uninit.init, aux) == false) {
							printf("[supplemental_page_table_copy] uninit vm_alloc is failed\n");
							free(aux);
							goto err;
						}
						//printf("[SPT copy] UNINIT claim start\n");
						// if(vm_claim_page(va) == false) {
						// 	printf("[supplemental_page_table_copy] uninit vm_claim_page failed\n");
						// 	goto err;
						// }
						break;}
					default:
						PANIC("[SPT COPY] UNKNOWN TYPE %d", type);
					}
					//copy parent frame to child frame
					if(type!=VM_UNINIT) {
						struct page* child_page = spt_find_page(dst, va);
						if(child_page==NULL) {
							goto err;
						}
						memcpy(child_page->frame->kva, page->frame->kva, PGSIZE);
					}
				}
				return true;
err:
	return false;

}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread */
	hash_clear(&spt->spt_hash, page_free);

	/* TODO: writeback all the modified contents to the storage. */
}

/* project 3 */
/* hash table functions */
/* Returns a hash value for page p. */
unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED) {
  const struct page *p = hash_entry (p_, struct page, hash_elem);
  return hash_bytes (&p->va, sizeof p->va);
}

/* Returns true if page a precedes page b. */
bool
page_less (const struct hash_elem *a_,
           const struct hash_elem *b_, void *aux UNUSED) {
  const struct page *a = hash_entry (a_, struct page, hash_elem);
  const struct page *b = hash_entry (b_, struct page, hash_elem);

  return a->va < b->va;
}

/* Returns the page containing the given virtual address, or a null pointer if no such page exists. */
struct page *
page_lookup (const void *va, struct supplemental_page_table *spt) {
  //printf("[page_lookup] start\n");
  struct page p;
  struct hash_elem *e;

  p.va = pg_round_down(va);
  e = hash_find (&spt->spt_hash, &p.hash_elem);
  return e != NULL ? hash_entry (e, struct page, hash_elem) : NULL;
}

/* Free the page containing the given hash elem, or a null pointer if no such page exists. The page will be freed by vm_dealloc_page because the actual page table(pml4) and the physical memory(palloc-ed memory) will be cleaned after SPT is cleaned up.*/
void
page_free(struct hash_elem* e, void* aux) {
	struct page* page = hash_entry(e, struct page, hash_elem);
	if(page == NULL) {
		printf("[page_free] hash_entry is failed\n");
		return false;
	}
	vm_dealloc_page(page);
}