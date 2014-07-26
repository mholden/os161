#include <types.h>
#include <syscall.h>
#include <kern/types.h>
#include <vnode.h>
#include <uio.h>
#include <lib.h>
#include <kern/errno.h>
#include <curthread.h>
#include <thread.h>

/*
 * The getdirentry() syscall.
 */

int sys_getdirentry(int filehandle, char *buf, size_t buflen, int *ret){
	int err, curr_offset;
	struct vnode *dir;
	struct uio u;

	assert(ret != NULL);
        if(filehandle < 0 || filehandle >= MAX_FILES_PER_THREAD || !(curthread->file_descriptors[filehandle])){
                *ret = -1;
                return EBADF;
        }
	if(buf == NULL){
		*ret = -1;
		return EINVAL;
	}

	dir = curthread->file_descriptors[filehandle]->file;
	curr_offset = curthread->file_descriptors[filehandle]->curr_offset;	

	/* Set up our uio struct */
        u.uio_iovec.iov_ubase = (userptr_t)buf;
        u.uio_iovec.iov_len = buflen;
        u.uio_resid = buflen;
        u.uio_offset = curr_offset;
        u.uio_segflg = UIO_USERSPACE;
        u.uio_rw = UIO_READ;
        u.uio_space = curthread->t_vmspace;

	err = VOP_GETDIRENTRY(dir, &u);
	if(err){
		*ret = -1;
		return err;
	}

	curthread->file_descriptors[filehandle]->curr_offset = u.uio_offset;

	*ret = buflen - u.uio_resid;
	return 0;
}
