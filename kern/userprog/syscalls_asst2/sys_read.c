#include <types.h>
#include <lib.h>
#include <syscall.h>
#include <vnode.h>
#include <kern/errno.h>
#include <kern/types.h>
#include <uio.h>
#include <synch.h>
#include <curthread.h>
#include <thread.h>
#include <fd.h>

/* 
 * The read() syscall.
 */

int sys_read(int filehandle, void *buf, size_t size, int *ret){
	struct vnode *file;
        off_t curr_offset;
        struct uio u;
        struct lock *lock;
        int err;

        /* Error check */
        if(filehandle < 0 || !(curthread->file_descriptors[filehandle])) return EBADF;
        if(buf == NULL) return EINVAL;

        file = curthread->file_descriptors[filehandle]->file;
        curr_offset = curthread->file_descriptors[filehandle]->curr_offset;

        /* Set up our uio struct */
        u.uio_iovec.iov_ubase = (userptr_t)buf;
        u.uio_iovec.iov_len = size;
        u.uio_resid = size;
        u.uio_offset = curr_offset;
        u.uio_segflg = UIO_USERSPACE;
        u.uio_rw = UIO_READ;
        u.uio_space = curthread->t_vmspace;

        /* Find and acquire the file lock, then do the write */
        err = get_file_lock(file, &lock);
        if(err) return err;

        lock_acquire(lock);
        err = VOP_READ(file, &u);
        lock_release(lock);
        if(err) return err;

        *ret = size - u.uio_resid;
        return 0;
}
