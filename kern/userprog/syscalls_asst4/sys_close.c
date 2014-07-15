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

	/* close_file does all the work here */
	close_file(filehandle);

	return 0;
}
