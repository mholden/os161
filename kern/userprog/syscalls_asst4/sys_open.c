#include <types.h>
#include <kern/errno.h>
#include <syscall.h>

/*
 *  The open() syscall.
 */

int sys_open(const char *filename, int flags){
	(void) filename;
	(void) flags;
	return EUNIMP;

	/*
	 * 1. Error check (valid pointer? valid flags?)
	 * 2. Allocate a file descriptor from curthread->fd array
	 * 3. Call vfs_open()
	 */
}
