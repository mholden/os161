#include <types.h>
#include <lib.h>
#include <uio.h>
#include <syscall.h>
#include <kern/errno.h>
#include <curthread.h>
#include <thread.h>
#include <vnode.h>

/* 
 * The write() syscall.
 */

int sys_write(int filehandle, const void *buf, size_t size){
	
	/*
	 * 1. Error check inputs.
	 * 2. Set up uio struct.
	 * 3. Call VOP_WRITE()
	 */

	struct uio u;
	int ret;

	/* Error check.. should error check buf here too, maybe? */
	if(filehandle < 0 || !(curthread->file_descriptors[filehandle])) return EBADF;

	/* Set up our uio struct */
	u.uio_iovec.iov_ubase = (userptr_t)buf;
        u.uio_iovec.iov_len = size;
        u.uio_resid = size;          
        u.uio_offset = curthread->file_descriptors[filehandle]->curr_offset;
        u.uio_segflg = UIO_USERSPACE; 
        u.uio_rw = UIO_WRITE;
        u.uio_space = curthread->t_vmspace;

	ret = VOP_WRITE(curthread->file_descriptors[filehandle]->file, &u);
	if(ret) return ret;

	/* Check u.uio_resid */
	if (u.uio_resid != 0) {
                kprintf("error in write() - u.uio_resid != 0\n");
                return -1; // not sure about this
        }	

	return size; // assume size bytes always written (since returning -1 above if this is not the case), but look into this
	


	// Old hack version below
	
	/*
	// For now just keep it simple.
	(void) filehandle;
	
	char *to_print;
	
	if(size == 1){ // we're getting called from putchar
		to_print = kmalloc(2);
		to_print[0] = ((char *)buf)[0];
		to_print[1] = '\0';
	}
	// assumes that if we're not being called from putchar(), buf will be null terminated. pretty hack..
	else to_print = (char *)buf;

	kprintf(to_print);
	if(size == 1) kfree(to_print);
	return 0; // Assume success.
	*/
}
