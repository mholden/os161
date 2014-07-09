#include <types.h>
#include <syscall.h>
#include <kern/errno.h>
#include <kern/types.h>
#include <kern/unistd.h>
#include <kern/stat.h>
#include <curthread.h>
#include <thread.h>
#include <vnode.h>

/* 
 * The lseek syscall.
 */

off_t sys_lseek(int filehandle, off_t pos, int code){
	
	/* 
	 * 1. error check inputs
	 * 2. based on code, adjust pos correctly, and call VOP_TRYSEEK
	 * 3. if tryseek returns w/o error, adjust offset in fd structure
	 */

	int err;
	off_t offset;
	struct stat statbuf;

	if(filehandle < 0 || !(curthread->file_descriptors[filehandle])) return EBADF;
	
	switch(code){
		case SEEK_SET:
			offset = pos;
			break;
		case SEEK_CUR:
			offset = curthread->file_descriptors[filehandle]->curr_offset + pos;
			break;
		case SEEK_END:
			err = VOP_STAT(curthread->file_descriptors[filehandle]->file, &statbuf);
			if(err) return -1; // What should I return here??
			offset = statbuf.st_size + pos;
			break;
		default:
			return EINVAL;
			break;
	}
	
	err = VOP_TRYSEEK(curthread->file_descriptors[filehandle]->file, offset);	
	if(err) return EINVAL;	

	curthread->file_descriptors[filehandle]->curr_offset = offset;
	return offset;
}
