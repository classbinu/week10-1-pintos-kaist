/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "userprog/process.h"
#include "include/threads/vaddr.h"

struct list frame_table; //프레임 테이블
struct list_elem *start; //프레임 테이블의 시작 주소

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
	list_init(&frame_table);
	start = list_begin(&frame_table);
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

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		struct page* page = (struct page*)malloc(sizeof(struct page));
		typedef bool (*initializerFunc)(struct page *, enum vm_type, void *);
		initializerFunc initializer = NULL;
		switch(VM_TYPE(type)) {
			case VM_ANON:
				initializer = anon_initializer;
				break;
			case VM_FILE:
				initializer = file_backed_initializer;
				break;
		}
		/* TODO: Insert the page into the spt. */
		uninit_new(page, upage, init, type, aux, initializer);
		//page number initialization
		page->writable = writable;
		// hex_dump(page->va, page->va, PGSIZE, true);
		return spt_insert_page(spt, page);
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
/* 인자로 받은 spt 내에서 va를 키로 전달해서 이를 갖는 page를 리턴한다.)
	hash_find(): hash_elem을 리턴해준다. 이로부터 우리는 해당 page를 찾을 수 있어.
	해당 spt의 hash 테이블 구조체를 인자로 넣자. 해당 해시 테이블에서 찾아야 하니까.
	근데 우리가 받은 건 va 뿐이다. 근데 hash_find()는 hash_elem을 인자로 받아야 하니
	dummy page 하나를 만들고 그것의 가상주소를 va로 만들어. 그 다음 이 페이지의 hash_elem을 넣는다.
	*/

	struct page *page = (struct page *)malloc(sizeof(struct page));
	struct hash_elem *e;
	
	page->va = pg_round_down(va); //해당 va가 속해있는 페이지 시작 주소를 갖는 page를 만든다.

	/* e와 같은 해시값을 갖는 page를 spt에서 찾은 다음 해당 hash_elem을 리턴 */

	e = hash_find(&spt->pages, &page->hash_elem);
	free(page);

	return e != NULL ? hash_entry(e, struct page, hash_elem): NULL;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	// int succ = false;
	// /* TODO: Fill this function. */
	// return succ;
	return page_insert(&spt->pages, page);
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
	struct thread *curr = thread_current();
	struct list_elem *e = start;

	//
	for (start = e; start != list_end(&frame_table); start = list_next(start)) {
		victim = list_entry(start, struct frame, frame_elem);
		if (pml4_is_accessed(curr->pml4, victim->page->va))
			pml4_set_accessed(curr->pml4, victim->page->va, 0);
		else
			return victim;
	}

	for (start = list_begin(&frame_table); start != e; start = list_next(start)) {
		victim = list_entry(start, struct frame, frame_elem);
		if (pml4_is_accessed(curr->pml4, victim->page->va))
			pml4_set_accessed(curr->pml4, victim->page->va, 0);
		else
			return victim;
	}
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
vm_get_frame (void)
{
	struct frame *frame = (struct frame *)malloc(sizeof(struct frame));

	//project3.1 : memory management
	
	frame->kva = palloc_get_page(PAL_USER);
	if(frame->kva == NULL) {
		frame = vm_evict_frame();//프레임 테이블에서 프레임을 빼옴
		frame->page = NULL;//프레임 테이블에서 빼온 프레임의 페이지를 NULL로 초기화
		return frame;
	}
	list_push_back(&frame_table, &frame->frame_elem);//프레임 테이블에 프레임을 넣음
	frame->page = NULL;//프레임 테이블에 넣은 프레임의 페이지를 NULL로 초기화

	ASSERT(frame != NULL);
	ASSERT(frame->page == NULL);
  
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
	// struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	if(is_kernel_vaddr(addr)) return false;
	if(!not_present) return false;
	if(!vm_claim_page(addr)) return false;

	return true;
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
	struct page *page = spt_find_page(&thread_current()->spt,va);
	if(page == NULL) return false;
	
	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page(struct page *page)
{
    struct frame *frame = vm_get_frame();

    /* Set links */
    frame->page = page;
    page->frame = frame;

    /* TODO: Insert page table entry to map page's VA to frame's PA. */
    // 가상 주소와 물리 주소를 매핑
    struct thread *current = thread_current();
    pml4_set_page(current->pml4, page->va, frame->kva, page->writable);

    return swap_in(page, frame->kva); // uninit_initialize
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	hash_init(&(spt->pages), page_hash, page_less, NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
			// Project 3.2_anonymous page
	struct hash_iterator i;
	struct hash *src_hash = &src->pages;
    struct hash *dst_hash = &dst->pages;
	hash_first(&i, src_hash);
	while(hash_next(&i))
	{
		struct page *src_page = hash_entry(hash_cur(&i), struct page, hash_elem);
		enum vm_type type = page_get_type(src_page);
		if(type == VM_UNINIT)
		{
			struct uninit_page *uninit_page = &src_page->uninit;
			struct container* container = (struct container *) uninit_page->aux;
			struct container* new_container = (struct container *) malloc(sizeof(struct container));
			new_container->file = file_duplicate(container->file); //file_duplicate()를 통해 파일을 복사

			vm_alloc_page_with_initializer(uninit_page->type, src_page->va, src_page->writable, file_backed_initializer, new_container);
			vm_claim_page(src_page->va);
		}
		else
		{
			vm_alloc_page(type, src_page->va, src_page->writable);
			vm_claim_page(src_page->va);
			memcpy(src_page->va,src_page->frame->kva, PGSIZE); //src_page->va에 src_page->frame->kva를 복사
		}
	}
	return true;
}

void spt_destructor(struct hash_elem *e, void* aux) {
    struct page* page = hash_entry(e, struct page, hash_elem);
    if (page != NULL) {
        destroy(page);  // 페이지에 할당된 자원을 해제하는 함수 호출
        free(page);  // 페이지 자체의 동적 메모리 해제
    }
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* 할 일: 스레드별로 보관 중인 보충_페이지_테이블을 모두 삭제하고 수정된 모든 내용을 스토리지에 다시 씁니다.  */
	 // hash_action_destroy 콜백 함수를 사용하여 각 엔트리의 자원을 해제
	hash_clear(&spt->pages, spt_destructor);
}


/*project 3*/

/*SPT에서 히값을 통해 value로 들어있는 페이지를 찾든, 테이블에 삽입하든 기본적으로
 hash함수가 있어야 가상 주소를 hashed indes로 변환 할 수 있기에 이를 해주는 함수임.*/
unsigned page_hash(const struct hash_elem *e, void *aux UNUSED)
{
	struct page *p = hash_entry(e, struct page, hash_elem);// 해당 page가 들어있는 해시 테이블 시작 주소를 가져옴
	return hash_bytes(&p->va, sizeof p->va);
}

/*
해시 테이블 초기화할 때 해시 요소들 비교하는 함수의 포인터
a가 b보다 작으면 true, 반대면 false
*/

bool page_less(const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED)
{
	struct page *a = hash_entry(a_, struct page, hash_elem);
	struct page *b = hash_entry(b_, struct page, hash_elem);
	return a->va < b->va;
}

/*
해당 페이지에 들어있는 hash_elem 구조체를 인자로 받은 해시 테이블에 삽입하는 함수
*/

bool page_insert(struct hash *h, struct page *p) {
	
    if(!hash_insert(h, &p->hash_elem))
		return true;
	else
		return false;

}

/*page를 해시 테이블에서 제거하는 함수*/
bool page_delete(struct hash *h, struct page *p) {
	if(!hash_delete(h, &p->hash_elem)) {
		return true;
	}
	else
		return false;

}


