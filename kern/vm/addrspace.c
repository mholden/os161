#include <types.h>
#include <kern/unistd.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <vm.h>
#include <vfs.h>
#include <machine/spl.h>
#include <machine/tlb.h>

/* Address space related functions */

struct addrspace *
as_create(char *progname)
{

	struct addrspace *as = kmalloc(sizeof(struct addrspace));
	if (as==NULL) {
		return NULL;
	}

	/* Initialize everything */
	int i, result;
	for(i = 0; i < TWO_LEV_PAGE_TABLE_SIZE; i++) as->page_directory[i] = NULL;

	as->progname = kmalloc(strlen(progname) + 1);
	strcpy(as->progname, progname);

	/* Open our own copy of the file */
	result = vfs_open(progname, O_RDONLY, &(as->progfile));
	if(result) return NULL;

	as->code = NULL;
	as->data = NULL;
	as->heap = NULL;
	as->user_heap = NULL;
	as->stack = NULL;

	as->done_loading_code_page = 0;

	return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	DEBUG(DB_VM, "In as_copy().\n");
	struct addrspace *new;
	int i, j;
	int result;
	paddr_t paddr;

	new = as_create(old->progname);
	if (new==NULL) {
		return ENOMEM;
	}
	
	for(i = 0; i < TWO_LEV_PAGE_TABLE_SIZE; i++){
		if(old->page_directory[i] != NULL){
			result = create_page_table(new, i); /* <-Delete this line for copy-on-write */
			if(result) return result; /* <-Delete this line for copy-on-write */
			/* Add this for copy-on-write
				new->page_directory[i] = old->page_directory[i];		 
			*/
			for(j = 0; j < TWO_LEV_PAGE_TABLE_SIZE; j++){
				if(old->page_directory[i]->entries[j] != NULL){
					/*
					 * This is just rough, and will all be replaced when copy-on-write is written.
					 */
					new->page_directory[i]->entries[j] = kmalloc(sizeof(struct page_table_entry));
					if(new->page_directory[i]->entries[j] == NULL) return ENOMEM;
					paddr = page_alloc(USER_ALLOC, (vaddr_t) ((i << 22) | (j << 12)), new);	//added this to prevent duplicates
					write_pte(new->page_directory[i]->entries[j], paddr, old->page_directory[i]->entries[j]->permission, -1);
					memmove((void *)PADDR_TO_KVADDR(paddr),
						(const void *)PADDR_TO_KVADDR(old->page_directory[i]->entries[j]->paddr),
						PAGE_SIZE);
					/* Delete above for copy-on-write */
					/* Add this for copy on write
						old->page_directory[i]->entries[j]->permission = READ_ONLY;
						#as_activate called immediately after this in thread_fork, which will flush the TLB
						#need to add our second mapping to the core map now (both 'new' and 'old' will be sharing
						 the position in the core map). if invalid, get it out from disk and put it in the core map
						 (instead of trying to edit things on disk.. just edit them in the core map.. instead of 
						 making it really complicated, just add two (or even just one) extra addrspace field to 
						 our core map pages), and write our 'new' addrspace to that field. that should be it#
					*/
				}
			}
		}
	}

	/* Deep copy all regions */
	new->code = kmalloc(sizeof(struct region));
	new->data = kmalloc(sizeof(struct region));
	new->heap = kmalloc(sizeof(struct region));
	new->user_heap = kmalloc(sizeof(struct region));
	new->stack = kmalloc(sizeof(struct region));
	*(new->code) = *(old->code);
	*(new->data) = *(old->data);
	*(new->heap) = *(old->heap);
	*(new->user_heap) = *(old->user_heap);
	*(new->stack) = *(old->stack);

	/* Will this work for copy-on-write? Yeah, won't even need to load code pages for copy-on-write */
	new->done_loading_code_page = old->done_loading_code_page;	
	
	*ret = new;
	return 0;
}

void
as_destroy(struct addrspace *as)
{
	int i, j;
	/* Free all memory */
	for(i = 0; i < TWO_LEV_PAGE_TABLE_SIZE; i++){
		if(as->page_directory[i] != NULL){
			for(j = 0; j < TWO_LEV_PAGE_TABLE_SIZE; j++){
				if(as->page_directory[i]->entries[j] != NULL) kfree(as->page_directory[i]->entries[j]);
			}
			kfree(as->page_directory[i]);
		}
	}
	vfs_close(as->progfile);
	kfree(as->progname);
	kfree(as->code);
	kfree(as->data);
	kfree(as->heap);
	kfree(as->user_heap);
	kfree(as->stack);
	free_all_user_pages(as);
	kfree(as);
}

void
as_activate(struct addrspace *as)
{
	int i, spl;

	(void)as;

	spl = splhigh();
	DEBUG(DB_VM, "TLB flush.\n");
	for (i=0; i<NUM_TLB; i++) {
		TLB_Write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission is set on the segment.
 */
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
		 int readable, int writeable, int executable)
{
	/* I don't use these */
	(void)readable;
	(void)executable;

	int npages;

	/* Align the region. First, the base... */
	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	/* ...and now the length. */
	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

	/* Simplified permissions - it's either writeable or read_only */
	int permission;

	if(writeable) permission = WRITEABLE;
	else permission = READ_ONLY;

	/* 
	 * I'm gunna assume that we're coming in here twice, and that the first
	 * time will be for the code section (read-only), and the second time 
	 * will be for the data section (read-write). I'm only assuming this 
	 * because that's the way it's been so far with all programs I've tested.
	 * I have asserts in here in case for some reason my assumption is not
	 * correct.
	 */

	/* First time - code */
	if(as->code == NULL){
		assert(as->data == NULL);
		assert(as->heap == NULL);
		assert(as->stack == NULL);

		/* Code section */
		as->code = kmalloc(sizeof(struct region));
		as->code->base = vaddr;
		as->code->top = vaddr + sz;
		as->code->permission = permission;
		return 0;
	} 
	/* Second time - data */
	else {
		assert(as->data == NULL);
		assert(as->heap == NULL);
		assert(as->stack == NULL);

		/* Data section */
		as->data = kmalloc(sizeof(struct region));
		as->data->base = vaddr;
		as->data->top = vaddr + sz;
		as->data->permission = permission;

		/* Number of pages required for data section */
		sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;
		npages = sz / PAGE_SIZE;
	
		/* Heap section */
		as->heap = kmalloc(sizeof(struct region));
		as->user_heap = kmalloc(sizeof(struct region));
		as->heap->base = as->heap->top = as->user_heap->base = as->user_heap->top = vaddr + (npages * PAGE_SIZE);
		as->heap->permission = as->user_heap->permission = WRITEABLE;
	
		/* Stack section defined in as_define_stack() */
		return 0;
	}

	/*
	 * Our assumption was incorrect.
	 */
	kprintf("VM: Warning: more regions than expected\n");
	return EUNIMP;
}

int
as_prepare_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */

	(void)as;
	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */

	(void)as;
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	as->stack = kmalloc(sizeof(struct region));
	*stackptr = as->stack->base = as->stack->top = USERSTACK;
	as->stack->permission = WRITEABLE;
	
	return 0;
}

