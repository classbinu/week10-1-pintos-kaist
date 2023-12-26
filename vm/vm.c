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
bool check_stack_boundary(uintptr_t rsp, void* fault_addr);
/* frame table for frame management*/
struct list frame_table;

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	//printf("[vm이름 긴 것] start\n");

	ASSERT (VM_TYPE(type) != VM_UNINIT)
	struct supplemental_page_table *spt = &thread_current ()->spt;
	/* Check wheter the upage is already occupied or not. */
	void* upage_va = upage;
	if (spt_find_page (spt, upage) == NULL) {
		//printf("[vm이름 긴 것] 찾는 주소: %p\n", upage_va);
		/* Create the page, fetch the initialier according to the VM type*/
		//struct page* new_page = calloc(1, sizeof(struct page));
		struct page* new_page = malloc(sizeof(struct page));
		if(new_page == NULL){
			goto err;
		}
		 /* TODO: and then create "uninit" page struct by calling uninit_new.*/ 
		//struct uninit_page* new_uninit_page;
		//uninit_new(new_page, upage_va, init, type, aux, NULL);

		 /* TODO: You should modify the field after calling the uninit_new. */
		//type이름 그대로 써넣기, uninit new삭제
		switch(VM_TYPE(type)) {
			case(VM_ANON):
				//printf("[vm이름 긴 것] ANON\n");
				uninit_new(new_page, upage_va, init, type, aux, anon_initializer);
				new_page->writable = writable;
				break;
			case(VM_FILE):
				//printf("[vm이름 긴 것] VM_FILE\n");
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
		//printf("[vm이름 긴 것] spt_insert_page passed\n");
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
	// struct thread* curr = thread_current();
	// struct list_elem* e, *start;

	// //순회를 돌며 가장 최근에 접근하지 않은 것들을 제거한다.
	// for (start = e; start != list_end(&frame_table); start = list_next(start)) {
	// 	victim = list_entry(start, struct frame, frame_elem);
	// 	if (pml4_is_accessed(curr->pml4, victim->page->va))
	// 		pml4_set_accessed(curr->pml4, victim->page->va, 0);
	// 	else
	// 		return victim;
	// }

	// for (start = list_begin(&frame_table); start != e; start = list_next(start)) {
	// 	victim = list_entry(start, struct frame, frame_elem);
	// 	if (pml4_is_accessed(curr->pml4, victim->page->va))
	// 		pml4_set_accessed(curr->pml4, victim->page->va, 0);
	// 	else
	// 		return victim;
	// }

	// why e/start?
	return victim;

}
/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */
	swap_out(victim->page);
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

/* 
하나 이상의 익명 페이지를 할당하여 스택 크기를 늘려 addr이 더 이상 오류 주소가 되지 않도록 합니다. 할당을 처리할 때 addr을 PGSIZE로 반내림해야 합니다.

대부분의 운영체제에는 스택 크기에 대한 절대적인 제한이 있습니다. 일부 OS에서는 사용자가 제한을 조정할 수 있습니다(예: 많은 Unix 시스템에서 ulimit 명령어 사용). 많은 GNU/Linux 시스템에서 기본 제한은 8MB입니다. 이 프로젝트의 경우 스택 크기를 최대 1MB로 제한해야 합니다.


*/
/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
	uintptr_t prd_addr = pg_round_down(addr);
	//alloc first
	// if(vm_alloc_page(VM_ANON|VM_MARKER_0, prd_addr, true)==NULL) {
	// 	printf("[vm_stack_growth] first if failed\n");
	// 	return false;
	// }
	//claim together
	// if(vm_claim_page(prd_addr)==NULL) {
	// 	printf("[vm_stack_growth] second if failed\n");
	// 	return false;
	// }
	// return true;

	vm_alloc_page(VM_ANON|VM_MARKER_0, prd_addr, true);
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/*
이 함수는 페이지 오류 예외를 처리하는 동안 userprog/exception.c의 page_fault에서 호출됩니다. 이 함수에서는 페이지 오류가 스택 증가에 유효한 경우인지 여부를 확인해야 합니다. 스택 증가로 오류를 처리할 수 있음을 확인했다면 오류가 발생한 주소로 vm_stack_growth를 호출합니다.
*/
/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;

	//printf("[vm try fault] start : addr:%p / user:%d / write:%d, not present:%d\n", addr, user, write, not_present);
	//printf("[vm try fault] curr->rsp: %p\n", thread_current()->intr_rsp);
	/* TODO: Validate the fault */

	// If addr is null
	if(addr == NULL) {
		//printf("[vm try fault error] addr == NULL\n");
		return false;
	}
	// If address is in kernel 
	if(is_kernel_vaddr(addr)) {
		//printf("[vm try fault error] is_kernel_vaddr\n");
		return false;
	}

	uintptr_t rsp = f->rsp;
		//printf("[vm try fault /before !user] user: %d, rsp:%p\n", user, rsp);
		
		if(!user) {
			/* user false: access by kernel.
			when kernel access occured,
			intr_frame's rsp !=user stack pointer
			so get it from saved rsp from thread. */
			//printf("[vm try fault] rsp is changed (f->rsp) => (curr->rsp)\n");
			rsp = thread_current()->intr_rsp;
		}
		//printf("[vm try fault /after !user] user: %d, rsp:%p\n", user, rsp);

	// if it is present but fault is occured
	if(!not_present){
		//printf("[vm try fault error]!not_present\n");
		return false;
	}
	// same with (not_present == 1)
	// if page is exist 
	if((page =spt_find_page(spt, addr))==NULL) {
		//printf("[vm try fault error] page: %p \n", page);
		//printf("[vm try fault error] if page is not exist in spt\n");

		if(check_stack_boundary(rsp, (uintptr_t)addr)) {
			//printf("[vm try fault] check stack boundary is passed\n");
			vm_stack_growth(addr);
			return true;
		}

		//printf("[vm try fault error] checking stack_boundary is finished\n");
		return false;
	}


	//printf("[vm try fault] page->writable: %d\n", page->writable);

	if(write==true &&page->writable==false) {
		//printf("[vm try fault error]write==true &&page->writable==false\n");
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

/* check address from rsp */
bool
check_stack_boundary (uintptr_t rsp, void* fault_addr) {
	/*
	fault_addr <= USER_STACK : USER STACK(시작점)아래 영역
	fault_addr >= USER_STACK - (1<<20)(=1MB) :최대 stack영역내 주소
	fault_addr >= rsp-8 : rsp이동 전에 exception이 나서 뜬 fault addr를 커버하기
	*/
	if((fault_addr <= USER_STACK) && (fault_addr >= USER_STACK - (1<<20)) && (fault_addr >= rsp-8)) {
		//모든 조건 만족하여 범위 내임을 확인하였다
		return true;
	}
	//printf("[check_stack_boundary] : not in boundary!\n");
	return false;
}