#include <types.h>
#include <kern/stat.h>
#include <kern/errno.h>
#include <syscall.h>
#include <vnode.h>
#include <curthread.h>
#include <thread.h>

/*
 * The fstat syscall.
 */

int sys_fstat(int fd, struct stat *buf){
	
	/* Make sure the file descriptor is valid */
	if(fd < 0 || !(curthread->file_descriptors[fd])) return EBADF;
	
	int ret;

	ret = VOP_STAT(curthread->file_descriptors[fd]->file, buf);
	return ret;
}
