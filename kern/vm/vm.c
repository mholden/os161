#include <types.h>
#include <kern/unistd.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <curthread.h>
#include <addrspace.h>
#include <vm.h>
#include <machine/spl.h>
#include <machine/tlb.h>
#include <synch.h>
#include <vnode.h>
#include <uio.h>
#include <elf.h>
#include <vfs.h>

/*
 * Our VM System
 */


/* Page struct for CoreMap */
struct page {
    /* Current user of the page */
    struct addrspace* as;
    vaddr_t va;

    /* Current page "state" */
    u_int32_t state;

    /*
     * for paging algorithm if user space page
     * if kernel page then using to tell how many pages were allocated :P
     */
    u_int32_t number;   // FIFO paging, so a time stamp will be suffice
};


/*Swap Page*/
struct swap_page{
	/* User address space of the page */
	struct addrspace* as;
	/* Virtual address of page	*/
	vaddr_t va;
};


/*flag setup once VM setup*/
int vm_init_flag = 0;
struct lock * CoreMapLock;
struct page * pages;
u_int32_t page_num, first_free_page;
u_int32_t page_counter = 0;
u_int32_t global_lastaddr; // for debugging purposes
int TLB_replacement_counter; // for debugging purposes


/* Swapping variables*/
struct swap_page *SwapTable;
struct lock * SwapTableLock;
int swap_file_init = 0;		/* flag to setup once swap_file created*/
struct vnode* swapfile;


/*
 * Initializes virtual memory
 * Creates a lock for coremap, checks how much space would be needed for coremap
 * Allocates the space required for coremap, and initializes it
 * Sets vm_init_flag to 1 showing that the vm is setup
 */
void
vm_bootstrap(void)
{
	u_int32_t firstaddr, lastaddr, freeaddr;
	CoreMapLock = lock_create("CoreMapLock");
	SwapTableLock = lock_create("SwapTableLock");
	//gets size of ram with lastaddr
	ram_getsize(&firstaddr, &lastaddr);
	//number of pages needed for coremap
	page_num = lastaddr / PAGE_SIZE;
	//sets up the coremap=pages pointer
	pages = (struct page*)PADDR_TO_KVADDR(firstaddr);
	//to know which paddr is actually free
	freeaddr = firstaddr + page_num * sizeof(struct page);
	SwapTable = (struct swap_page*)PADDR_TO_KVADDR(freeaddr);
	freeaddr +=  SWAPTABLE_SIZE * sizeof(struct swap_page);
	u_int32_t i;
	//setting up first free page
	if (freeaddr % PAGE_SIZE == 0)
		first_free_page = freeaddr / PAGE_SIZE;
	else{
		first_free_page = freeaddr / PAGE_SIZE + 1;
	}

	kprintf("Available memory to start: %dk\n"
		"Total pages in coremap: %d\n"
		"Available pages to start: %d\n"
		"First free page to start: %d\n"
		"First free paddr to start: 0x%x\n",
		(lastaddr - freeaddr) / 1024, page_num, (lastaddr - freeaddr) / PAGE_SIZE, first_free_page, (paddr_t) (first_free_page * PAGE_SIZE));

	/*
	 *  Okay, looks good.
	 *  pages - a pointer (virtual) to the start of our core map
	 *  first_free_page - first available page above all the kernel code
	 *  page_num - total number of pages in our core map
	 */

	//loop through coremap to initialize page structs
	for (i = 0; i < page_num; i++){
		if (i < first_free_page){
			pages[i].state = PAGE_FIXED;
			pages[i].va = PADDR_TO_KVADDR(i * PAGE_SIZE);
		}
		else{
			pages[i].state = PAGE_FREE;
			pages[i].va = 0;
		}
		pages[i].as = NULL;
		pages[i].number = 0;
	}

	//loop through SwapTable to initialize it
	for (i = 0; i < SWAPTABLE_SIZE; i++){
		SwapTable[i].as = NULL;
		SwapTable[i].va = 0;

	}
	//set flag
	vm_init_flag = 1;

	//variables for debugging
	global_lastaddr = lastaddr;
	TLB_replacement_counter = 0;
}

int
find_region (vaddr_t faultaddress, struct addrspace *as){
	/* Determine our region, and fault if not a valid address */
		if (faultaddress >= as->code->base && faultaddress < as->code->top) {
			return CODE_REGION;
		}
		else if (faultaddress >= as->data->base && faultaddress < as->data->top) {
			return DATA_REGION;
		}
		else if (faultaddress >= as->heap->base && faultaddress <= as->heap->top) {
			if (faultaddress == as->stack->base){
				/* Should probably just return EFAULT here */
				panic("Stack overflow - not yet supported.\n");
			}
			return HEAP_REGION;
		}
		else if (faultaddress >= (as->stack->base - PAGE_SIZE) && faultaddress < as->stack->top) {
			return STACK_REGION;
		}
		return -1;
}


int
swapout_page(int page_num, int *swap_entry ){
	int result;
	struct uio u;
	struct addrspace *as;
	int region;
	vaddr_t va;
	//this is only called from page_alloc or page_nalloc so assuming I have lock
	as = pages[page_num].as;
	va = pages[page_num].va;

	lock_acquire(SwapTableLock);
	//kprintf("In swapout\n");
	if (!swap_file_init){
		//first page to swap
		//swapfile has no been opened yet
		//kprintf("In swapout.....2\n");
		int err = vfs_open("lhd0raw:", O_RDWR, &swapfile);
		if (err != 0) {
		    kprintf("In vfs_open error\n");
		    return err;
		}
		//kprintf("In swapout.....3\n");
		/*
		 * Allocate the page in the first available space on the file
		 */
		//lock_acquire(SwapTableLock);
		SwapTable->as = as;
		SwapTable->va = va;
		//lock_release(SwapTableLock);
		/*
		 * File IO
		 */
		//original uio for kernel
		///*
		mk_kuio(&u, PADDR_TO_KVADDR(page_num * PAGE_SIZE), PAGE_SIZE, 0, UIO_WRITE);
		//*/
		//uio for userspace addr
		/*
		region = find_region (va, as);
		if ( region < 0) {
				return EFAULT;
		}

		u.uio_iovec.iov_ubase = (userptr_t)va;
		u.uio_iovec.iov_len = PAGE_SIZE;   // length of the memory space
		u.uio_resid = PAGE_SIZE;          // amount to actually write
		u.uio_offset = 0;
		u.uio_segflg = (region == CODE_REGION) ? UIO_USERISPACE : UIO_USERSPACE;
		u.uio_rw = UIO_WRITE;
		u.uio_space = as;
		*/
		/*
		u.uio_iovec.iov_kbase = (void *)PADDR_TO_KVADDR(page_num * PAGE_SIZE);
		u.uio_iovec.iov_len = PAGE_SIZE;   // length of the memory space
		u.uio_resid = PAGE_SIZE;          // amount to actually write
		u.uio_offset = 0;
		u.uio_segflg = UIO_SYSSPACE;
		u.uio_rw = UIO_WRITE;
		u.uio_space = NULL;
		 */
		result = VOP_WRITE(swapfile, &u);
		if (result) {
			return result;
		}
		//kprintf("In swapout.....5\n");
		if (u.uio_resid != 0) {
			/* short write; problem with file? */
			kprintf("ELF: short write\n");
			return EIO;
		}
		//kprintf("In swapout.....6\n");
		*swap_entry = 0;
		swap_file_init = 1;
	}
	else{
		int i;
		//lock_acquire(SwapTableLock);
		//swap file already opened
		for(i = 0; i < SWAPTABLE_SIZE; i++){
			if (SwapTable[i].as == as && SwapTable[i].va == va)
				break;
		}
		if (i == SWAPTABLE_SIZE){
			//look for free page in disk
			for(i = 0; i < SWAPTABLE_SIZE; i++){
				if (SwapTable[i].as == NULL && SwapTable[i].va == 0){
					SwapTable[i].as = as;
					SwapTable[i].va = va;
					break;
				}
			}
		}
		//lock_release(SwapTableLock);
		if ( i ==  SWAPTABLE_SIZE)
			panic("Ran out of pages in swap file.\n");

		//original uio for kernel
		///*
		mk_kuio(&u, PADDR_TO_KVADDR(page_num * PAGE_SIZE), PAGE_SIZE, (i * PAGE_SIZE), UIO_WRITE);
		//*/
		//uio for userspace addr
		/*
		region = find_region (va, as);
		if ( region < 0) {
			return EFAULT;
		}

		u.uio_iovec.iov_ubase = (userptr_t)va;
		u.uio_iovec.iov_len = PAGE_SIZE;   // length of the memory space
		u.uio_resid = PAGE_SIZE;          // amount to actually write
		u.uio_offset = (i * PAGE_SIZE);
		u.uio_segflg = (region == CODE_REGION) ? UIO_USERISPACE : UIO_USERSPACE;
		u.uio_rw = UIO_WRITE;
		u.uio_space = as;
		 */
		/*
		u.uio_iovec.iov_kbase = (void *)PADDR_TO_KVADDR(page_num * PAGE_SIZE);
		u.uio_iovec.iov_len = PAGE_SIZE;   // length of the memory space
		u.uio_resid = PAGE_SIZE;          // amount to actually write
		u.uio_offset = (i * PAGE_SIZE);
		u.uio_segflg = UIO_SYSSPACE;
		u.uio_rw = UIO_WRITE;
		u.uio_space = NULL;
		 */
		result = VOP_WRITE(swapfile, &u);
		if (result) {
			return result;
		}
		if (u.uio_resid != 0) {
			/* short write; problem with file? */
			kprintf("ELF: short write\n");
			return EIO;
		}

		*swap_entry = i;

	}

	lock_release(SwapTableLock);
	pages[page_num].state = PAGE_CLEAN;
	//(DB_VM, "In swapout end, swap_entry: %d, state: %d, as: %x, va: %x, page %d\n", *swap_entry, pages[page_num].state, as, va, page_num);
	//kprintf("In swapout.....7\n");

	//kprintf("In swapout end, swap_entry: %d, state: %d, as: %d, va: %d\n", *swap_entry, pages[page_num].state, as, va);
	return 0;
}


void
evict_page(int page_num, int swap_index){
	//kprintf("In evict_page, swap_index: %d, page_num: %d\n", swap_index, page_num);
	struct addrspace *as;
	vaddr_t va;
	int i = -1;
	as = pages[page_num].as;
	va = pages[page_num].va;
	/* Search for the address in TLB*/
	//kprintf("In evict.....1\n");
	if (as == curthread->t_vmspace){
		i = TLB_Probe(va,0);
		//(DB_VM, "In evict page, TLB_PROBE %d\n", i);
	}
	if (i >= 0){
		//kprintf("In evict.....i >= 0\n");
		TLB_Write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}
	//kprintf("In evict.....1\n");
	int dir_index = va >> 22;
	int pag_table_index = (va & PAGE_TABLE_MASK) >> 12;
	as->page_directory[dir_index]->entries[pag_table_index]->swap_entry = swap_index;
	as->page_directory[dir_index]->entries[pag_table_index]->valid = 0;
	//kprintf("In evict.....2\n");
	pages[page_num].state = PAGE_FREE;
	pages[page_num].va = 0;
	pages[page_num].as = NULL;
	pages[page_num].number = 0;
}



static
paddr_t
getppages(unsigned long npages)
{
	int spl;
	paddr_t addr;

	spl = splhigh();
	addr = ram_stealmem(npages);

	splx(spl);

	return addr;
}


paddr_t
page_alloc(int alloc_type, vaddr_t vaddr, struct addrspace *as)
{
		paddr_t addr;

						//not sure if needs to be interrupts high
						//but if there is a CS that uses a page that was chosen to be deleted
						//Then I think problems would arise...
						//Maybe just interrupts high while we choose and clear page would be good
						//but I think the lock would take care of it since to access coremap
						//the lock will always be required
		lock_acquire(CoreMapLock);
		u_int32_t i;
		u_int32_t page_selected = 0;
		// Not using this right now, and it's giving warnings.
		u_int32_t page_time_min = 4294967295;		//I hope it gives a very large number :P		*/
		int swap_index;
		//DEBUG(DB_VM, "						In page_alloc, va x%x, as x%x type %d\n", vaddr, as, alloc_type);
		for (i = first_free_page; i < page_num; i++){
			if(pages[i].state == PAGE_FREE){
				page_selected = i;
				break;
			}
			/* Not thinking about swapping at all, for now */

			else if (pages[i].number < page_time_min && pages[i].state != PAGE_FIXED){
				page_time_min = pages[i].number;
				page_selected = i;
			}

		}
		if (i == page_num){		//if i == page_num then no free page found
						//no free pages so must swap....
			/* Not doing anything here, yet. Just panic, for now.. */
			//panic("Ran out of pages. No swapping support yet.\n");
			//kprintf("swapping starting, page selected %d\n", page_selected);
			DEBUG(DB_VM, "					In page_alloc, need to swap page_selected # %d, va x%x, as x%x\n",
					page_selected, pages[page_selected].va, pages[page_selected].as);
			/*
			 * If page is dirty then need to flush it to disk
			 */
			if (pages[page_selected].state == PAGE_FIXED) panic("EVERY SINGLE PAGE IS FIXED\n");
			if (pages[page_selected].state == PAGE_DIRTY){
				/*
				 * need to create function to flush to disk
				 * not sure how to do it now
				 * TO BE DONE!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
				 */
				//kprintf("Page dirty\n");
				swapout_page(page_selected, &swap_index);
				//page_flush(page_selected);
			}
			/*
			 * If page is CLEAN then it means that it is updated to disk already
			 */
			evict_page(page_selected, swap_index);
			/*
			 * Now update page table or TLB to show that page_selected is no longer mapped to memory
			 * PageTables are not setup yet so need to update...
			 * Need function to update TLB and page table
			 * TO BE DONE!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
			 */
			//kprintf("Should be done with swapping....\n");
		}

		/*
		 * Okay, we have a free page and want to give it to whoever is asking for it.
		 * Update the page in our core map, and return the physical address. 		 
		 */

		/* Here's the address to return */
		addr = page_selected * PAGE_SIZE;
	
		/* Update the page. */
		pages[page_selected].as = as;

		if(alloc_type == KERNEL_ALLOC){
			/* Mark the page as fixed, and initialize everything else */
			pages[page_selected].va = PADDR_TO_KVADDR(addr);
			pages[page_selected].state = PAGE_FIXED;
			pages[page_selected].number = 1;

			DEBUG(DB_VM, "Just gave page at: 0x%x to kernel: 0x%x virtual address: 0x%x\n"
				     "Pages left (before we need to start swapping): %d.\n", 
				addr, (u_int32_t) pages[page_selected].as, pages[page_selected].va, ((global_lastaddr - addr)/4096) - 1);
		}
		else{
			/* alloc_type should be USER_ALLOC, then. Nothing else is supported. */
			assert(alloc_type == USER_ALLOC);
			assert(pages[page_selected].as != NULL);
			
			pages[page_selected].va = vaddr;
			pages[page_selected].state = PAGE_DIRTY;
			pages[page_selected].number = page_counter;
			page_counter++;
			
			DEBUG(DB_VM, "Just gave page at: 0x%x to user: 0x%x virtual address: 0x%x\n"
				     "Pages left (before we need to start swapping): %d.\n", 
				addr, (u_int32_t) pages[page_selected].as, pages[page_selected].va, ((global_lastaddr - addr)/4096) - 1);
		}

		//test, print coremap
		/*
		int test_i;
		for (test_i = 0; test_i < page_num; test_i++){
			kprintf("index %d, va %h, as %h, state %d, number %d\n",test_i,pages[test_i].va,pages[test_i].as, pages[test_i].state, pages[test_i].number);
		}
		*/
		lock_release(CoreMapLock);

		return addr;
}


paddr_t
page_nalloc(int alloc_type, int npages)
{

		paddr_t base_addr;

						//not sure if needs to be interrupts high
						//but if there is a CS that uses a page that was chosen to be deleted
						//Then I think problems would arise...
						//Maybe just interrupts high while we choose and clear page would be good
						//but I think the lock would take care of it since to access coremap
						//the lock will always be required
		lock_acquire(CoreMapLock);
		int i, subpage_index, all_free_flag, fixed_flag;
		int page_selected = 0;
		u_int32_t combined_pages_time;
		u_int32_t pages_time_min = 4294967295;		//I hope it gives a very large number :P
		int last_page = (page_num - npages + 1);
		int swap_index;

		/* 
		 * Right now, this only looks for n pages *in a row*. For user level mallocs,
		 * it doesn't necessarily need to be this way (can allocate a bunch of random
		 * pages, not necessarily back to back, as long as we set things up in the
		 * page table correctly). We need to add user-level support.
		 */
		for (i = first_free_page; i < last_page; i++){	//taking into account npages needed
			combined_pages_time = 0;
			fixed_flag = 0;		//flag to tell if there was a fixed page
			all_free_flag = 1;	//flag to tell if all the pages were free
			for (subpage_index = 0; subpage_index < npages; subpage_index++){
				if (pages[i + subpage_index].state == PAGE_FIXED){
					fixed_flag = 1;
					break;
				}
				if (pages[i + subpage_index].state != PAGE_FREE){
					all_free_flag = 0;
					combined_pages_time += pages[i + subpage_index].number;
				}
			}
			if(!fixed_flag){
				if(all_free_flag){
					page_selected = i;
					break;
				}
				else if (combined_pages_time < pages_time_min){
					pages_time_min = combined_pages_time;
					page_selected = i;
				}
			}
		}
	if (i == last_page){		//if i == last_num then no free page group found

		/* Not doing anything here, yet. Just panic, for now.. */
		//panic("Ran out of pages. No swapping support yet.\n");

		for (subpage_index = 0; subpage_index < npages; subpage_index++){

			/*
			 * If page is dirty then need to flush it to disk
			 */
			if (pages[page_selected + subpage_index].state == PAGE_DIRTY){
			/*
			 * need to create function to flush to disk
			 * not sure how to do it now
			 * TO BE DONE!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
			 */
				swapout_page((page_selected + subpage_index), &swap_index);
			//page_flush(page_selected);
			}
			if (pages[page_selected + subpage_index].state == PAGE_CLEAN){
			/*
			 * If page is CLEAN then it means that it is updated to disk already
			 */
				evict_page((page_selected + subpage_index), swap_index);
			/*
			 * Now update page table or TLB to show that page_selected is no longer mapped to memory
			 * PageTables are not setup yet so need to update...
			 * Need function to update TLB and page table
			 * TO BE DONE!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
			 */
			//function to unload AS or something like that...

			//function to set as invalid in TLB & point to Disk where stored

			//function to set page invalid in PageTable and point to Disk where stored

			//function to zero the Page
			}
		}
	}

	/* Here's the address to return */
	base_addr = page_selected * PAGE_SIZE;

	if(alloc_type == KERNEL_ALLOC){
		for (subpage_index = 0; subpage_index < npages; subpage_index++){
			pages[page_selected + subpage_index].as = curthread->t_vmspace;
			pages[page_selected + subpage_index].number = npages;
			pages[page_selected + subpage_index].state = PAGE_FIXED;
			pages[page_selected + subpage_index].va = PADDR_TO_KVADDR(base_addr + (subpage_index * PAGE_SIZE));	
		}
		DEBUG(DB_VM, "Just gave %d pages at base addr: 0x%x to kernel: 0x%x virtual address: 0x%x\n"
			     "Pages left (before we need to start swapping): %d.\n", 
				npages, base_addr, (u_int32_t) pages[page_selected].as, pages[page_selected].va, ((global_lastaddr - base_addr)/4096) - 				npages);
	} else {
		/* DON'T NEED THIS! USER MALLOC ONLY EVER CALLS PAGE_ALLOC */
	
		/* alloc_type should be USER_ALLOC, then. Nothing else is supported. */
		assert(alloc_type == USER_ALLOC);
		/* Panic for now */
		panic("User alloc_npages() not support yet.\n");

		/* Don't forget to deal with page counter in here, for swapping purposes */	
	}

	lock_release(CoreMapLock);

	return base_addr;
}


/* Allocate some kernel-space virtual pages */
vaddr_t
alloc_kpages(int npages)
{
	paddr_t pa;
	if (vm_init_flag){
		if(npages == 1) pa = page_alloc(KERNEL_ALLOC, 0, NULL);
		else pa = page_nalloc(KERNEL_ALLOC, npages);
	}
	else pa = getppages(npages);
	if (pa==0) {
		return 0;
	}
	return PADDR_TO_KVADDR(pa);
}

/* Free all user pages of the address space. */
void
free_all_user_pages(struct addrspace *as)
{
	lock_acquire(CoreMapLock);
	int i;
	/* Find them in the coremap */
	for (i = first_free_page; i < (int) page_num; i++){
		if((pages[i].as) == as && (pages[i].va <= 0x80000000) && (pages[i].state != PAGE_FREE)){
			/* Free it (if it isn't already free) */
			pages[i].state = PAGE_FREE;
			DEBUG(DB_VM, "Just freed page at physical addr: 0x%x from user: 0x%x virtual address: 0x%x\n", 
					(i * PAGE_SIZE), (u_int32_t) pages[i].as, pages[i].va);
		}
	}
	lock_release(CoreMapLock);
}

void
page_free(int free_type, vaddr_t vaddr){
	/* 
	 * Will start by supporting only kernel frees, and will add user frees when malloc() and
	 * free() are implemented 
	 */
	int i, j;
	lock_acquire(CoreMapLock);
	if(free_type == KERNEL_FREE){
		/* Our vaddr better be a kernel virtual address */
		assert(vaddr >= 0x80000000)

		if(vaddr < PADDR_TO_KVADDR(first_free_page*PAGE_SIZE)){
			/* These pages are all fixed, anyways. Don't worry about it */
			lock_release(CoreMapLock);
			return;
		} else {
			/* 
			 * Our kernel pages are all allocated 'in a row', so if there were
			 * multiple of them allocated at once, they'll be back to back in 
			 * our coremap. Free them all.
			 */
			
			/* Find it in the coremap */
			for (i = first_free_page; i < (int) page_num; i++){
				if(pages[i].va == vaddr) break;
			}
			if(i == (int) page_num){
				/* 
				 * It wasn't even in the core map. Kernel is trying to free an
				 * address that was never even allocated. Do nothing. 
				 */
				lock_release(CoreMapLock);
				return;
			}
			for(j = i; j < (i + (int) pages[i].number); j++){
				/* Free them */
				pages[j].state = PAGE_FREE;
			}
			DEBUG(DB_VM, "Just freed %d pages at base addr: 0x%x from kernel: 0x%x virtual address: 0x%x\n", 
					(int) pages[i].number, (i * PAGE_SIZE), (u_int32_t) pages[i].as, pages[i].va);
		}
	} else {
		assert(free_type == USER_FREE);
		panic("Trying to free user pages in page_free(). Not yet supported.\n");
	}
	lock_release(CoreMapLock);
	return;
}

/* Free some kernel-space virtual pages */
void
free_kpages(vaddr_t vaddr)
{
	/* 
	 * Okay, this will have pretty similar structure to alloc_kpages, we're just freeing
	 * now, not allocating.
	 */	

 	/* If vm_init_flag is set, we have our core map set up and are managing all the memory */  

	if(vm_init_flag) page_free(KERNEL_FREE, vaddr);

	/* 
	 * If not, we're still very early in boot(). Too much of a hassle to even deal with it,
	 * so I'll just do nothing (memory will be wasted, but it won't be much, if any).
	 */
}

static
int
load_page(vaddr_t faultaddr, int region){
	struct vnode *v;
	int result, i;
	Elf_Ehdr eh;
	Elf_Phdr ph;
	struct uio ku;

	/* 
	 * This will always be happening from our own thread. ie. using 
	 * curthread should be okay.
	 */
	v = curthread->t_vmspace->progfile;

	/* The rest is pretty much copying and pasting from loadelf.c.. */

	mk_kuio(&ku, &eh, sizeof(eh), 0, UIO_READ);
	result = VOP_READ(v, &ku);
	if (result) {
		return result;
	}

	if (ku.uio_resid != 0) {
		/* short read; problem with executable? */
		kprintf("ELF: short read on header - file truncated?\n");
		return ENOEXEC;
	}

	/* 
	 * We've already verified this .elf file is okay in loadelf(), so skip
	 * the error checking.
	 */

	for (i=0; i<eh.e_phnum; i++) {
		off_t offset = eh.e_phoff + i*eh.e_phentsize;
		mk_kuio(&ku, &ph, sizeof(ph), offset, UIO_READ);

		result = VOP_READ(v, &ku);
		if (result) {
			return result;
		}

		if (ku.uio_resid != 0) {
			// short read; problem with executable?
			kprintf("ELF: short read on phdr - file truncated?\n");
			return ENOEXEC;
		}

		switch (ph.p_type) {
		    case PT_NULL:  continue;
		    case PT_PHDR: continue;
		    case PT_MIPS_REGINFO: continue;
		    case PT_LOAD: break;
		    default:
			kprintf("loadelf: unknown segment type %d\n", 
				ph.p_type);
			return ENOEXEC;
		}

		/* 
		 * Now check: is our faultaddr in the range of this segment?
		 * If so, this is the segment to load from, else try again.
		 */
		if(faultaddr >= ph.p_vaddr && faultaddr <= (ph.p_vaddr + ph.p_memsz)){
			/* We're in the right segment */
			if(ph.p_memsz > PAGE_SIZE){
				/*
				 * We're loading into pages of memory (of size PAGE_SIZE, obviously),
				 * so don't want to try to load any more than PAGE_SIZE bytes in. We
				 * kind of have to break this segment down into further segments, and
				 * then load the correct piece in. I'll probably change all of this 
				 * when I introduce 'regions' in our address space.
				 */

				/* 
			 	 * This assert is an assumption I'm making, but I don't think it necessarily needs to be true.
			 	 * If it ever causes problems, let me know and I'll change this code. I think it's like this:
				 * filesz is the actual number of bytes stored in the file, memsz is the amount of virtual
				 * memory the running program expects to see. I'm pretty sure I can just take this assertion	
				 * out and it would be fine.
			 	 */
				//assert(ph.p_filesz == ph.p_memsz);

				ph.p_offset += (faultaddr - ph.p_vaddr);
				if(faultaddr > (ph.p_vaddr + ph.p_filesz)) ph.p_filesz = 0;
				else if((ph.p_filesz - (faultaddr - ph.p_vaddr)) < PAGE_SIZE){
					ph.p_filesz -= (faultaddr - ph.p_vaddr);
				}
				else ph.p_filesz = PAGE_SIZE;
			}
			result = load_segment(v, ph.p_offset, faultaddr, 
				      PAGE_SIZE, ph.p_filesz,
				      ph.p_flags & PF_X);
			if(result) return result;
			break;
		}
	}

	if(region == CODE_REGION) curthread->t_vmspace->done_loading_code_page = 1;
	return 0;
}

void
write_pte(struct page_table_entry* pte, paddr_t paddr, int permission, int swap_entry){
	pte->paddr = paddr;
	pte->permission = permission;
	pte->valid = 1;
	pte->swap_entry = swap_entry;
}

int
create_page_table(struct addrspace* as, int page_dir_index)
{
	/* Create and initialize the page table */
	DEBUG(DB_VM, "VM: kmallocing a page_table.\n");
	as->page_directory[page_dir_index] = kmalloc(sizeof(struct page_table));
	if(as->page_directory[page_dir_index] == NULL) return ENOMEM;
	
	int i;
	for(i = 0; i < TWO_LEV_PAGE_TABLE_SIZE; i++) as->page_directory[page_dir_index]->entries[i] = NULL;

	return 0;
}

static
struct page_table_entry*
create_page_table_entry(struct addrspace* as, vaddr_t vaddr)
{
	int result;
	/* 
	 * Has the necessary page table even been created in our page
	 * directory yet? If not, create it.
	 */
	int page_dir_index = vaddr >> 22;
	if(as->page_directory[page_dir_index] == NULL){
		result = create_page_table(as, (int) page_dir_index);
		if(result) return NULL;
	};

	/*
	 * Create the page table entry.
	 */
	int page_table_index = (vaddr & PAGE_TABLE_MASK) >> 12;
	as->page_directory[page_dir_index]->entries[page_table_index] = kmalloc(sizeof(struct page_table_entry));
	return as->page_directory[page_dir_index]->entries[page_table_index];
}

static
struct page_table_entry*
check_page_table(struct addrspace* as, vaddr_t faultaddr)
{
	/* 
	 * Has the necessary page table even been created in our page
	 * directory yet? Check.
	 */
	int page_dir_index = faultaddr >> 22;
	if(as->page_directory[page_dir_index] == NULL) return NULL;

	/*
	 * Okay, it has. All entries initialized to NULL, so returns 
	 * NULL if the actual entry isn't in there yet.
	 */
	int page_table_index = (faultaddr & PAGE_TABLE_MASK) >> 12;
	return as->page_directory[page_dir_index]->entries[page_table_index];
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	paddr_t paddr;
	int i, result;
	u_int32_t ehi, elo;
	struct addrspace *as;
	struct page_table_entry* pte;
	int spl;
	int region, permission;
	int first_time, done_before, done_after, just_wrote_code_page; /* Flags for TLB and on-demand paging stuff */
	int swap_index = -1;

	/* Bad idea to kprintf in here apparently..
	kprintf("in vm_fault.. frame we're faulting on: 0x%x\n", faultaddress);
	*/

	spl = splhigh();
	
	/* Initialize all flags */
	first_time = 0;	
	done_before = 0;
	done_after = 0;
	just_wrote_code_page = 0;

	faultaddress &= PAGE_FRAME;

	//DEBUG(DB_VM, "VM: fault: 0x%x\n", faultaddress);

	switch (faulttype) {
	    case VM_FAULT_READONLY:
	    case VM_FAULT_READ:
	    case VM_FAULT_WRITE:
		break;
	    default:
		DEBUG(DB_VM, "User mode fault 1: %x\n", as);
		splx(spl);
		return EINVAL;
	}

	as = curthread->t_vmspace;
	if (as == NULL) {
		/*
		 * No address space set up. This is probably a kernel
		 * fault early in boot. Return EFAULT so as to panic
		 * instead of getting into an infinite faulting loop.
		 */
		DEBUG(DB_VM, "User mode fault 2: %x\n", as);
		return EFAULT;
	}

	/* Error check */
	assert(as->code != NULL);
	assert(as->data != NULL);
	assert(as->heap != NULL);
	assert(as->stack != NULL);
	assert((as->code->base & PAGE_FRAME) == as->code->base);
	assert((as->data->base & PAGE_FRAME) == as->data->base);
	assert((as->heap->base & PAGE_FRAME) == as->heap->base);
	assert((as->heap->top & PAGE_FRAME) == as->heap->top);
	assert((as->stack->base & PAGE_FRAME) == as->stack->base);
	assert((as->stack->top & PAGE_FRAME) == as->stack->top);
	
	region = find_region (faultaddress, as);


	if ( region < 0) {
		DEBUG(DB_VM, "User mode fault 3: %x\n", as);
		splx(spl);
		return EFAULT;
	}

	if(region == CODE_REGION) permission = READ_ONLY;
	else permission = WRITEABLE;

	if(permission == READ_ONLY && faulttype == VM_FAULT_READONLY){
		/* This is a real READ_ONLY fault (not copy-on-write) */
		DEBUG(DB_VM, "User mode fault 4: %x\n", as);
		splx(spl);
		return EFAULT;
	}

	pte = check_page_table(as, faultaddress);
	if(pte == NULL){
		/* 
		 * The address is not yet mapped, and therefore has not
		 * yet been loaded into memory. Create the page table entry,
		 * grab a page from memory, write the mapping, and set the 
		 * first_time flag.
		 */

 		pte = create_page_table_entry(as, faultaddress);
		if(pte == NULL) {
			DEBUG(DB_VM, "User mode fault 5: %x\n", as);
			splx(spl);			
			return ENOMEM;
		}
		paddr = page_alloc(USER_ALLOC, faultaddress, as);
		write_pte(pte, paddr, permission, -1);
		first_time = 1;
	}
	else if (pte->valid)
		paddr = pte->paddr;
	else{
			//not valid PTE but not NULL, so it must have been swapped
			swap_index = pte->swap_entry;		//index of swap saved as address
			/*
			lock_acquire(SwapTableLock);
			if (SwapTable[swap_index].as != as || SwapTable[swap_index].va != faultaddress)
				panic("SwapTable does not match to value in page table");
			lock_release(SwapTableLock);
			*/
			//DEBUG(DB_VM, "swapped page requested back %d\n", swap_index);
			//panic("swapped page requested back %d\n", swap_index);
			paddr = page_alloc(USER_ALLOC, faultaddress, as);		//allocate a page
			write_pte(pte, paddr, permission, swap_index);
			/*
			 * File IO will be needed
			 */
			struct uio u;
			//DEBUG(DB_VM, "swapped page request paddr %x\n", paddr);

			mk_kuio(&u, PADDR_TO_KVADDR(paddr), PAGE_SIZE, (swap_index * PAGE_SIZE), UIO_READ);
			/*
			u.uio_iovec.iov_ubase = (userptr_t)faultaddress;
			u.uio_iovec.iov_len = PAGE_SIZE;   // length of the memory space
			u.uio_resid = PAGE_SIZE;          // amount to actually read
			u.uio_offset = (swap_index * PAGE_SIZE);
			u.uio_segflg = (region == CODE_REGION) ? UIO_USERISPACE : UIO_USERSPACE;
			u.uio_rw = UIO_READ;
			u.uio_space = curthread->t_vmspace;
			*/
			lock_acquire(SwapTableLock);
			result = VOP_READ(swapfile, &u);
			lock_release(SwapTableLock);
			if (result) {
				DEBUG(DB_VM, "User mode fault 6: %x\n", as);
				splx(spl);
				return result;
			}
			if (u.uio_resid != 0) {
				/* short read; problem with file? */
				DEBUG(DB_VM, "User mode fault 7: %x\n", as);
				splx(spl);
				return EIO;
			}
			//DEBUG(DB_VM, "Seems swapin went fine\n");
	}



	if(region == CODE_REGION && first_time) as->done_loading_code_page = 0;

	/* make sure it's page-aligned */
	assert((paddr & PAGE_FRAME)==paddr);

	/* 
	 * Alright, at this point we should have it mapped. Put it in the TLB. 
	 * First set up 'ehi' and 'elo'.
	 */
	
	/* Look for an empty spot in the TLB */
	for (i=0; i<NUM_TLB; i++) {
		TLB_Read(&ehi, &elo, i);
		if (elo & TLBLO_VALID) {
			continue;
		}
		ehi = faultaddress;
		/* Most of the time, we'll do : */
		if(pte->permission == READ_ONLY) elo = paddr | TLBLO_VALID;
		else elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
		/* But, : */
		if((pte->permission == READ_ONLY) && !as->done_loading_code_page){
			/* We're writing the *code* (normally read-only) into memory */
			elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
			/* Make it writeable, write to it, then flush the TLB immediately (below) */
		}
		/* If copy on write: */
		if(faulttype == VM_FAULT_READONLY){
			/* We already returned EFAULT if it was an 'actual' READ_ONLY fault */
			panic("Copy-on-write not yet supported. We shouldn't have even gotten here.\n");
			/* Add this for copy on write
				# both us and the guy we copied from are sharing the page table and memory
				# okay, core map entry now has two addrspace fields (swap page entry will need
				  both as well.. page_alloc will need to be edited to take in both addrspace
				  fields as arguments, rather than just using curthread->t_vmspace). #
				# now, two addrspace fields in core map, get them. if their page directories
				  at the index of interest are identical, we'll need to create a new page table, 
				  copy all *read_only* entries over (other entries may have been created in between..
				  we don't wanna copy those), make entry of faultaddress WRITEABLE for both, allocate
				  a new (unshared page) for us in the core map, and set up the mapping. else, if their
				  page directory index is not identical, just make entries WRITEABLE and set the mapping,
				  but may need an additional flag here because page table could have been created for
				  another fault not to do with any of our copied-on-write addresses.. holy shit.. this 
				  might be too much work to even be worth it #
			*/
		}
		//DEBUG(DB_VM, "VM: 0x%x -> 0x%x, index %d\n", faultaddress, paddr, i);
		TLB_Write(ehi, elo, i);
		break;
	}

	/* If none empty, shove it in randomly (for now) */
	if(i == NUM_TLB){
		//DEBUG(DB_VM, "TLB full, replacing randomly.\n");
		TLB_replacement_counter++;
		ehi = faultaddress;
		/* Most of the time, we'll do : */
		if(pte->permission == READ_ONLY) elo = paddr | TLBLO_VALID;
		else elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
		/* But, : */
		if((pte->permission == READ_ONLY) && !as->done_loading_code_page){
			/* We're writing the *code* (normally read-only) into memory */
			elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
			/* Make it writeable, write to it, then flush the TLB immediately (below) */
		}
		/* If copy on write: */
		if(faulttype == VM_FAULT_READONLY){
			/* We already returned EFAULT if it was an 'actual' READ_ONLY fault */
			panic("Copy-on-write not yet supported. We shouldn't have even gotten here.\n");
		}
		TLB_Random(ehi, elo);
	}

	/* 
	 * Okay, at this point, we've done a lot of the work. 
	 * 
	 * If we're dealing with a TLB_WRITE fault, we don't need to load anything.
	 * The memory has been allocated, and the mapping has been set up. Our user 
	 * process should now be able to write to the address which caused the fault.
	 *
	 * If it's a TLB_READ fault, we need to load the data (most likely instructions
	 * or data from the .text or .data sections of the .elf file) into the page of
	 * memory we just allocated and mapped.
	 */

	if((region == CODE_REGION || region == DATA_REGION) && first_time){
		done_before = as->done_loading_code_page;
		result = load_page(faultaddress, region);
		if(result){
			DEBUG(DB_VM, "User mode fault 8: %x\n", as);
			splx(spl);
			return result;
		}
		done_after = as->done_loading_code_page;
		just_wrote_code_page = done_after - done_before;
		if(just_wrote_code_page){
			DEBUG(DB_VM, "VM: Just wrote code page: 0x%x, addrspace: 0x%x\n", faultaddress, (u_int32_t) as);
			/* Flush the TLB */
			//DEBUG(DB_VM, "TLB flush.\n", faultaddress, (u_int32_t) as);
			for (i=0; i<NUM_TLB; i++) 
				TLB_Write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
		}
	}
	if((region == HEAP_REGION || region == STACK_REGION) && first_time){
		if((faultaddress >= as->heap->top) && (faultaddress < as->stack->base)){
			if(region == STACK_REGION){			
				/* Move our stack base around as necessary */
				assert((as->stack->base - faultaddress) == PAGE_SIZE);
				as->stack->base = faultaddress;
				/* Kill the process if it's stack gets too large */
				if((as->stack->top - as->stack->base) > USER_STACK_MAX) return EFAULT;
				DEBUG(DB_VM, "VM: Stack of addrspace: 0x%x just grew down to base: 0x%x\n", (u_int32_t) as, as->stack->base);
			}
			else{
				/* Move our heap top around as necessary */
				assert(region == HEAP_REGION);
				assert(faultaddress == as->heap->top);
				as->heap->top = faultaddress + PAGE_SIZE;
				DEBUG(DB_VM, "VM: Heap of addrspace: 0x%x just grew up to top: 0x%x\n", (u_int32_t) as, as->heap->top);
			}
		}
	}
	
	splx(spl);
	return 0;
}
