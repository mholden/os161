#include <types.h>
#include <lib.h>
#include <uio.h>
#include <syscall.h>
#include <kern/errno.h>
#include <kern/types.h>
#include <kern/unistd.h>
#include <curthread.h>
#include <thread.h>
#include <vnode.h>
#include <synch.h>
#include <fd.h>

/* 
 * The write() syscall.
 */

int sys_write(int filehandle, const void *buf, size_t size, int *ret){
	struct vnode *file;
	off_t curr_offset;
	struct uio u;
	int err, rw_flags;
	
	/* Error check */
	if(filehandle < 0 || !(curthread->file_descriptors[filehandle])) return EBADF;
	if(buf == NULL) return EINVAL;
	rw_flags = curthread->file_descriptors[filehandle]->rw_flags;
        if(!((rw_flags == O_WRONLY) || (rw_flags == O_RDWR))) return EINVAL;

	file = curthread->file_descriptors[filehandle]->file;
	curr_offset = curthread->file_descriptors[filehandle]->curr_offset;

	/* Set up our uio struct */
	u.uio_iovec.iov_ubase = (userptr_t)buf;
        u.uio_iovec.iov_len = size;
        u.uio_resid = size;          
        u.uio_offset = curr_offset;
        u.uio_segflg = UIO_USERSPACE; 
        u.uio_rw = UIO_WRITE;
        u.uio_space = curthread->t_vmspace;

	/* Find and acquire the file lock, then do the write */
	err = acquire_file_lock(file);
	if(err) return err;

	err = VOP_WRITE(file, &u);
	release_file_lock(file);
	if(err) return err;

	/* Update our offset */
	curthread->file_descriptors[filehandle]->curr_offset = u.uio_offset;

	*ret = size - u.uio_resid;
	return 0;
}
