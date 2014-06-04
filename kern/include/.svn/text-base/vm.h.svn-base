#ifndef _VM_H_
#define _VM_H_

#include <machine/vm.h>

/*
 * VM system-related definitions.
 */

struct addrspace;
struct page_table_entry;

/* Flag set after VM has been initialized (vm_bootstrap()) */
extern int vm_init_flag;
extern int TLB_replacement_counter;

/* Fault-type arguments to vm_fault() */
#define VM_FAULT_READ        0    /* A read was attempted */
#define VM_FAULT_WRITE       1    /* A write was attempted */
#define VM_FAULT_READONLY    2    /* A write to a readonly page was attempted*/

/* The maximum size of a user process's stack - even this is generous */
#define USER_STACK_MAX	262144

#define	SWAPTABLE_SIZE		2048

/* Page states */
#define PAGE_FREE	0
#define PAGE_DIRTY	1
#define PAGE_CLEAN	2
#define PAGE_FIXED	3

/* For alloc_page() and alloc-npages() */
#define USER_ALLOC	0
#define KERNEL_ALLOC	1

/* For free_pages() */
#define USER_FREE	0
#define KERNEL_FREE	1

/* Code regions */
#define CODE_REGION	0
#define DATA_REGION	1
#define HEAP_REGION	2
#define STACK_REGION 	3

/* Initialization function */
void vm_bootstrap(void);

/* Fault handling function called by trap code */
int vm_fault(int faulttype, vaddr_t faultaddress);

/* Allocate/free kernel heap pages (called by kmalloc/kfree) */
vaddr_t alloc_kpages(int npages);

/* Allocate/free (a) page(s) of physical memory */
paddr_t page_alloc(int alloc_type, vaddr_t vaddr, struct addrspace *as);
paddr_t page_nalloc(int alloc_type, int npages);
void page_free(int free_type, vaddr_t vaddr);
void free_all_user_pages(struct addrspace *as);

/* Page table functions */
void write_pte(struct page_table_entry* pte, paddr_t paddr, int permission, int swap_entry);
int create_page_table(struct addrspace* as, int page_dir_index);

/*flush a dirty page to disk*/
void page_flush( int page_num);
void free_kpages(vaddr_t addr);

#endif /* _VM_H_ */
