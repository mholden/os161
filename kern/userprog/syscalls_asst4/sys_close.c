#include <types.h>
#include <kern/errno.h>
#include <syscall.h>

/*
 *  The close() syscall.
 */

int sys_close(int fd){
	(void) fd;
	return EUNIMP;
}
