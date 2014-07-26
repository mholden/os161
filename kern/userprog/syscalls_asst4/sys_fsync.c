#include <types.h>
#include <syscall.h>
#include <vnode.h>
#include <kern/errno.h>
#include <curthread.h>
#include <thread.h>

/*
 * The fsync() syscall.
 */

int sys_fsync(int filehandle, int *ret){
	int err;
	struct vnode *file;

	if(ret == NULL) return EINVAL;
	if(filehandle < 0 || !(curthread->file_descriptors[filehandle])){
		*ret = -1;
		return EBADF;
	}

	file = curthread->file_descriptors[filehandle]->file;
	err = VOP_FSYNC(file);
	if(err){
		*ret = -1;
		return err;
	}

	*ret = 0;
	return 0;
}
