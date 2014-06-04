#ifndef _ADDRSPACE_H_
#define _ADDRSPACE_H_

#include <vm.h>
#include "opt-dumbvm.h"

struct vnode;

#define TWO_LEV_PAGE_TABLE_SIZE 	1024		// 2^10
#define USER_HEAP_MAX			1048576 	// 1MB

#define READ_ONLY	0
#define WRITEABLE	1

/* Address space regions */
struct region {
	vaddr_t base;
	vaddr_t top;
	int permission;
};



/* Bits for page table entry*/
#define PTE_VALID		0x00000001
#define PTE_MODIFY		0x00000002
#define PTE_REFERENCE		0x00000004
#define PTE_RW			0x00000008
#define PTE_EXECUTABLE		0x00000010


/* 
 * Page table entry - data structure associated with the page table
 * entries within our page tables.
 */
struct page_table_entry {
	paddr_t paddr;
	int permission;
	int valid;
	int swap_entry;
	/*
	 * Add more here (valid bit, etc.) when necessary.
	 */
};	

/* 
 * Page table - data structure associated with the page tables
 * pointed to by our page directory.
 */
struct page_table {
	struct page_table_entry* entries[TWO_LEV_PAGE_TABLE_SIZE];
};


/* 
 * Address space - data structure associated with the virtual memory
 * space of a process.
 */

struct addrspace {
	struct page_table* page_directory[TWO_LEV_PAGE_TABLE_SIZE];
	char* progname;
	struct vnode* progfile;
	/* Address space regions */
	struct region* code;
	struct region* data;
	struct region* heap;
	struct region* user_heap;
	struct region* stack;
	/* A flag for TLB stuff */
	int done_loading_code_page; 
};

/*
 * Functions in addrspace.c:
 *
 *    as_create - create a new empty address space. You need to make 
 *                sure this gets called in all the right places. You
 *                may find you want to change the argument list. May
 *                return NULL on out-of-memory error.
 *
 *    as_copy   - create a new address space that is an exact copy of
 *                an old one. Probably calls as_create to get a new
 *                empty address space and fill it in, but that's up to
 *                you.
 *
 *    as_activate - make the specified address space the one currently
 *                "seen" by the processor. Argument might be NULL, 
 *		  meaning "no particular address space".
 *
 *    as_destroy - dispose of an address space. You may need to change
 *                the way this works if implementing user-level threads.
 *
 *    as_define_region - set up a region of memory within the address
 *                space.
 *
 *    as_prepare_load - this is called before actually loading from an
 *                executable into the address space.
 *
 *    as_complete_load - this is called when loading from an executable
 *                is complete.
 *
 *    as_define_stack - set up the stack region in the address space.
 *                (Normally called *after* as_complete_load().) Hands
 *                back the initial stack pointer for the new process.
 */

struct addrspace *as_create(char *progname);
int               as_copy(struct addrspace *src, struct addrspace **ret);
void              as_activate(struct addrspace *);
void              as_destroy(struct addrspace *);

int               as_define_region(struct addrspace *as, 
				   vaddr_t vaddr, size_t sz,
				   int readable, 
				   int writeable,
				   int executable);
int		  as_prepare_load(struct addrspace *as);
int		  as_complete_load(struct addrspace *as);
int               as_define_stack(struct addrspace *as, vaddr_t *initstackptr);

/*
 * Functions in loadelf.c
 *    load_elf - load an ELF user program executable into the current
 *               address space. Returns the entry point (initial PC)
 *               in the space pointed to by ENTRYPOINT.
 */

int load_elf(struct vnode *v, vaddr_t *entrypoint);
int load_segment(struct vnode *v, off_t offset, vaddr_t vaddr, size_t memsize, size_t filesize, int is_executable);

#endif /* _ADDRSPACE_H_ */
