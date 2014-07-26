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

off_t sys_lseek(int filehandle, off_t pos, int code, int *ret){
	int err;
	off_t offset;
	struct stat statbuf;

	if(filehandle < 0 || !(curthread->file_descriptors[filehandle])){
		*ret = -1;
		return EBADF;
	}
	
	switch(code){
		case SEEK_SET:
			offset = pos;
			break;
		case SEEK_CUR:
			offset = curthread->file_descriptors[filehandle]->curr_offset + pos;
			break;
		case SEEK_END:
			err = VOP_STAT(curthread->file_descriptors[filehandle]->file, &statbuf);
			if(err){ 
				*ret = -1;
				return err;
			}
			offset = statbuf.st_size + pos;
			break;
		default:
			*ret = -1;
			return EINVAL;
			break;
	}
	
	err = VOP_TRYSEEK(curthread->file_descriptors[filehandle]->file, offset);	
	if(err){
		*ret = -1;
		return err;	
	}

	curthread->file_descriptors[filehandle]->curr_offset = offset;
	
	*ret = offset;
	return 0;
}
