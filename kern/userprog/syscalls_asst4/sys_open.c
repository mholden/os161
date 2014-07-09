#include <types.h>
#include <lib.h>
#include <kern/errno.h>
#include <syscall.h>
#include <vfs.h>
#include <vnode.h>
#include <fd.h>

/*
 *  The open() syscall.
 */

int sys_open(const char *filename, int flags){
	
	/* Should error check inputs (valid pointer? valid flags?) */
	int err, ret;
	struct vnode *file = NULL; 

	/* This may destroy filename.. copy it somewhere if needed */
	err = vfs_open(filename, flags, &file);
	if(err)	return err;

	ret = acquire_fd(file);
	return ret;
}
