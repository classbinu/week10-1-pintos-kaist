#ifndef VM_VM_H
#define VM_VM_H
#include <stdbool.h>
#include "include/lib/kernel/list.h"
#include "include/lib/kernel/hash.h"
#include "threads/palloc.h"

enum vm_type {
	/* page not initialized */
	VM_UNINIT = 0,
	/* page not related to the file, aka anonymous page */
	VM_ANON = 1,
	/* page that realated to the file */
	VM_FILE = 2,
	/* page that hold the page cache, for project 4 */
	VM_PAGE_CACHE = 3,

	/* Bit flags to store state */

	/* Auxillary bit flag marker for store information. You can add more
	 * markers, until the value is fit in the int. */
	VM_MARKER_0 = (1 << 3),
	VM_MARKER_1 = (1 << 4),

	/* DO NOT EXCEED THIS VALUE. */
	VM_MARKER_END = (1 << 31),
};

#include "vm/uninit.h"
#include "vm/anon.h"
#include "vm/file.h"

#ifdef EFILESYS
#include "filesys/page_cache.h"
#endif

struct page_operations;
struct thread;

#define VM_TYPE(type) ((type) & 7)

/** "페이지"의 표현입니다.
 * 일종의 "부모 클래스"로, 다음과 같은 네 개의 "자식 클래스"가 있습니다.
 * uninit_page, file_page, anon_page, 그리고 페이지 캐시(project4).
 * 이 구조의 미리 정의된 멤버를 제거/수정하지 마세요. * */
struct page {
	const struct page_operations *operations;
	void *va;              /* Address in terms of user space */
	struct frame *frame;   /* Back reference for frame */

	/* Your implementation */

	/*project 3*/
	struct hash_elem helem; //해시 테이블의 원소를 나타내는 구조체


	// int ref_count; //해당 페이지를 참조하는 프로세스의 개수
	// unsigned long flags; //페이지의 상태를 나타내는 플래그
	// struct list_head* lru; //연결된 목록 또는 트리 노드를 나타내는 구조체 포인터
	// struct vm_area_struct* vma; //해당 페이지가 속한 가상 주소 공간을 나타내는 구조체 포인터
	// unsigned long age; //연령 정보를 나타내는 변수 (LRU 알고리즘에서 사용)
	// unsigned long swap_slot; //스왑 슬롯 번호를 나타내는 변수
	// int * pte; //페이지 테이블 엔트리를 나타내는 변수

	//내 구현...?

	/* 유형별 데이터가 유니온에 바인딩됩니다.
	 * 각 함수는 현재 유니온을 자동으로 감지합니다.*/
	union {
		struct uninit_page uninit;
		struct anon_page anon;
		struct file_page file;
#ifdef EFILESYS
		struct page_cache page_cache;
#endif
	};
};

/* The representation of "frame" */
struct frame {
	void *kva;
	struct page *page;
};

/* The function table for page operations.
 * This is one way of implementing "interface" in C.
 * Put the table of "method" into the struct's member, and
 * call it whenever you needed. */
struct page_operations {
	bool (*swap_in) (struct page *, void *);
	bool (*swap_out) (struct page *);
	void (*destroy) (struct page *);
	enum vm_type type;
};

/* page의 operations 구조체 내 swap_in 함수 포인터를 이용해 해당 함수를 호출하며, page와 v를 인자로 전달*/
#define swap_in(page, v) (page)->operations->swap_in ((page), v)

/* page의 operations 구조체 내 swap_out 함수 포인터를 이용해 해당 함수를 호출하며, page를 인자로 전달*/
#define swap_out(page) (page)->operations->swap_out (page)

/* page의 operations 구조체 내 destroy 함수 포인터를 이용해 해당 함수를 호출하며, page를 인자로 전달*/
#define destroy(page) \
	if ((page)->operations->destroy) (page)->operations->destroy (page)

/* 현재 프로세스의 메모리 공간을 나타냅니다. 
 * 이 구조체의 특정 설계를 따르도록 강요하고 싶지 않습니다.
 * 모든 설계는 여러분의 몫입니다. */
struct supplemental_page_table {
	struct hash spt_hash; /* Supplemental page table hash table */

};

#include "threads/thread.h"
void supplemental_page_table_init (struct supplemental_page_table *spt);
bool supplemental_page_table_copy (struct supplemental_page_table *dst,
		struct supplemental_page_table *src);
void supplemental_page_table_kill (struct supplemental_page_table *spt);
struct page *spt_find_page (struct supplemental_page_table *spt,
		void *va);
bool spt_insert_page (struct supplemental_page_table *spt, struct page *page);
void spt_remove_page (struct supplemental_page_table *spt, struct page *page);

void vm_init (void);
bool vm_try_handle_fault (struct intr_frame *f, void *addr, bool user,
		bool write, bool not_present);

#define vm_alloc_page(type, upage, writable) \
	vm_alloc_page_with_initializer ((type), (upage), (writable), NULL, NULL)
bool vm_alloc_page_with_initializer (enum vm_type type, void *upage,
		bool writable, vm_initializer *init, void *aux);
void vm_dealloc_page (struct page *page);
bool vm_claim_page (void *va);
enum vm_type page_get_type (struct page *page);

#endif  /* VM_VM_H */
