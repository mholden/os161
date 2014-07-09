#include <types.h>
#include <kern/errno.h>
#include <syscall.h>
#include <curthread.h>
#include <thread.h>
#include <vnode.h>
#include <fd.h>
#include <vfs.h>

/*
 *  The close() syscall.
 */

int sys_close(int filehandle){
	
	if(filehandle < 0 || !(curthread->file_descriptors[filehandle])) return EBADF;

	vfs_close(curthread->file_descriptors[filehandle]->file);
	
	/* 
	 * Like vfs_open(), I'll assume the file system code takes care of either freeing or
	 * 'reclaiming' the vnode, and will just worry about freeing my fd structure.
	 */

	release_fd(filehandle);

	return 0;
}
