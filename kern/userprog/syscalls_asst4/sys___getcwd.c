#include <types.h>
#include <syscall.h>
#include <kern/types.h>
#include <uio.h>
#include <kern/errno.h>
#include <curthread.h>
#include <thread.h>
#include <vfs.h>

/*
 * The getcwd() syscall.
 */

int sys___getcwd(char *buf, size_t buflen, int *ret){
	int err;
	struct uio u;	

	if(ret == NULL) return EINVAL;
        if(buf == NULL){
                *ret = -1;
                return EINVAL;
        }

	/* Set up our uio struct */
        u.uio_iovec.iov_ubase = (userptr_t)buf;
        u.uio_iovec.iov_len = buflen;
        u.uio_resid = buflen;
        u.uio_offset = 0;
        u.uio_segflg = UIO_USERSPACE;
        u.uio_rw = UIO_READ;
        u.uio_space = curthread->t_vmspace;

	err = vfs_getcwd(&u);
	if(err){
		*ret = -1;
		return err;
	}
	
	*ret = buflen - u.uio_resid;
	return 0;
}
