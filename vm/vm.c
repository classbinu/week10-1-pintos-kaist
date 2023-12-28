/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
/* project 3 */
#include "lib/kernel/hash.h"
#include "threads/vaddr.h"
#include "userprog/process.h"

/* frame table for frame management*/
struct list frame_table;
struct lock frame_table_lock;

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
	lock_init(&frame_table_lock);
	list_init(&frame_table);
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
bool check_stack_boundary(uintptr_t rsp, void* fault_addr);

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
		struct page* new_page = malloc(sizeof(struct page));
		if(new_page == NULL){
			goto err;
		}
		 /* TODO: and then create "uninit" page struct by calling uninit_new.*/ 
		 /* TODO: You should modify the field after calling the uninit_new. */

		struct load_info* load_info = aux;
		bool (*page_initializer)(struct page *, enum vm_type, void *);
        page_initializer = NULL;

		switch(VM_TYPE(type)) {
			case(VM_ANON):
				page_initializer = anon_initializer;
				break;
			case(VM_FILE):
				page_initializer = file_backed_initializer;
				break;
			} 
		uninit_new(new_page, upage_va, init, type, load_info, page_initializer);
		new_page->writable = writable;

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
struct list_elem *policy_ref;
/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	
	struct frame *victim = NULL;
	//  /* TODO: The policy for eviction is up to you. */
	/* this policy occurs error in test case: swap-iter! */
	struct thread *curr = thread_current();
    policy_ref = list_begin(&frame_table);
    
    lock_acquire(&frame_table_lock);
    for (policy_ref; policy_ref != list_end(&frame_table);
         policy_ref = list_next(policy_ref)) {
        victim = list_entry(policy_ref, struct frame, frame_elem);
        // if bit is 1
        if (pml4_is_accessed(curr->pml4, victim->page->va)) {
            pml4_set_accessed(curr->pml4, victim->page->va, 0);
        } else {
            lock_release(&frame_table_lock);
            return victim;
        }
    }

    struct list_elem *iter = list_begin(&frame_table);

    for (iter; iter != list_end(&frame_table); iter = list_next(iter)) {
        victim = list_entry(iter, struct frame, frame_elem);
        // if bit is 1
        if (pml4_is_accessed(curr->pml4, victim->page->va)) {
            pml4_set_accessed(curr->pml4, victim->page->va, 0);
        } else {
            lock_release(&frame_table_lock);
            return victim;
        }
    }
    
    lock_release(&frame_table_lock);
    ASSERT(policy_ref != NULL);
    return victim;
	

}
/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */
	swap_out(victim->page);
	return victim;
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

	if(frame->kva == NULL) {
		frame = vm_evict_frame();
		frame->page = NULL;
		return frame;
	}

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);

	lock_acquire(&frame_table_lock);
	list_push_back(&frame_table, &frame->frame_elem);
	lock_release(&frame_table_lock);

	return frame;
}


/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
	uintptr_t prd_addr = pg_round_down(addr);
	vm_alloc_page(VM_ANON|VM_MARKER_0, prd_addr, true);
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

	uintptr_t rsp = f->rsp;
		
		if(!user) {
			/* user false: access by kernel.
			when kernel access occured,
			intr_frame's rsp !=user stack pointer
			so get it from saved rsp from thread. */
			rsp = thread_current()->intr_rsp;
		}

	// if it is present but fault is occured
	if(!not_present){
		return false;
	}
	// same with (not_present == 1)
	// if page is exist 
	if((page =spt_find_page(spt, addr))==NULL) {
		if(check_stack_boundary(rsp, (uintptr_t)addr)) {
			vm_stack_growth(addr);
			return true;
		}

		return false;
	}


	if(write==true &&page->writable==false) {
		return false;
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
int claim_counter = 0;
/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {

	//(1) get the frame
	struct frame *frame = vm_get_frame ();

	if(frame == NULL) {
		return false;
	}

	/* Set links */
	frame->page = page;
	page->frame = frame;
	
	
	bool res = pml4_set_page(thread_current()->pml4, page->va, frame->kva, page->writable);
	
	return swap_in (page, frame->kva);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	struct hash* target_ht = &spt->spt_hash;
	if(! hash_init(target_ht, page_hash, page_less, NULL)) {
		return NULL;
	}

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
							goto err;
						}
						if(!vm_claim_page(va)) {
							goto err;
						}
						break;
					case VM_FILE:
						if(!vm_alloc_page(VM_FILE, va, page->writable)) {
							goto err;
						}
						if(vm_claim_page(va) == false) {
							goto err;
						}
						break;
					//uninit page
					case VM_UNINIT:
						{//aux 
						void* aux = malloc(sizeof(struct load_info));
						if(aux == NULL) {
							goto err;
						}
						memcpy(aux, page->uninit.aux,sizeof(struct load_info));
						//page alloc
						if(vm_alloc_page_with_initializer(page->uninit.type, va, page->writable, page->uninit.init, aux) == false) {
							free(aux);
							goto err;
						}

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
	/* TODO: writeback all the modified contents to the storage. */
	
	hash_clear(&spt->spt_hash, page_free);

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
		return false;
	}
	vm_dealloc_page(page);
}

/* check address from rsp */
bool
check_stack_boundary (uintptr_t rsp, void* fault_addr) {
	/*
	fault_addr <= USER_STACK : addr from the USER STACK(start point) 
	fault_addr >= USER_STACK - (1<<20)(=1MB)  inside of max stack size's addr
	fault_addr >= rsp-8 : cover the fault addr occured by exception before rsp movment.
	*/

	if((fault_addr <= USER_STACK) && (fault_addr >= USER_STACK - (1<<20)) && (fault_addr >= rsp-8)){ 
		return true;
	}
	return false;
}