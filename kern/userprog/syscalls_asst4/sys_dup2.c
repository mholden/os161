#include <types.h>
#include <syscall.h>
#include <kern/errno.h>
#include <curthread.h>
#include <thread.h>
#include <fd.h>

/*
 * The dup2() syscall.
 */

int sys_dup2(int filehandle, int newhandle, int *ret){
	int err;

	if(ret == NULL) return EINVAL;
	if(filehandle < 0 || !(curthread->file_descriptors[filehandle]) || newhandle < 0){
                *ret = -1;
                return EBADF;
        }
	if(newhandle == filehandle) goto done;

	if(curthread->file_descriptors[newhandle]){
		/* newhandle already being used.. close it */
		close_file(newhandle);
	}

	err = dup_fd(filehandle, newhandle);
	if(err){
		*ret = -1;
		return err;
	}

done:
	*ret = newhandle;
	return 0;
}
