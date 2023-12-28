#ifndef VM_ANON_H
#define VM_ANON_H
#include "vm/vm.h"
struct page;
enum vm_type;

/* VM : swap i/o */
#define SECTOR_CNT 8
#define DISK_SECTOR_SIZE 512
#define BITMAP_ERROR SIZE_MAX

struct anon_page {
    /* Index of sector 
       -1 : in memory 
       else: in disk(sector)
    */
    int swap_index;
};

void vm_anon_init (void);
bool anon_initializer (struct page *page, enum vm_type type, void *kva);

#endif
