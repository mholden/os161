#include <types.h>
#include <kern/errno.h>
#include <thread.h>
#include <curthread.h>
#include <addrspace.h>
#include <vm.h>
#include <syscall.h>

/* 
 * The sbrk() syscall.
 */

void* sys_sbrk(intptr_t amount){
	struct addrspace *as;
	vaddr_t old_top;

	/* Align amount (so that MIPS won't get angry) */
	if((amount % 4) != 0) amount = ((amount + 4) / 4) * 4;

	as = curthread->t_vmspace;

	/* 
	 * Error check. I just return NULL for any error at this point. I'll return whatever
	 * the testers want us to return when I run the tests.
	 */
	if((as->user_heap->top + amount - as->user_heap->base) > USER_HEAP_MAX)	return (void*)-1;
	else if((as->user_heap->top + amount) < as->user_heap->base) return (void*)-2;

	/* Alright, we're aligned and the amount is valid */
	old_top = as->user_heap->top;
	as->user_heap->top += amount;
	
	/* Adjust kernel heap top if necessary */
	if(as->user_heap->top >= (as->heap->top + PAGE_SIZE)){
		as->heap->top += ((as->user_heap->top - as->heap->top) / PAGE_SIZE) * PAGE_SIZE;
		if(as->heap->top > as->stack->base){
			/* Stack overflow */
			return (void*)-1;
		}
	}

	return (void *)old_top;
}
