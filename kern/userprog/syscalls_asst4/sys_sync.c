#include <types.h>
#include <syscall.h>
#include <kern/errno.h>
#include <vfs.h>

/*
 * The sync() syscall.
 */

int sys_sync(int *ret){
	int err;

	if(ret == NULL) return EINVAL;

	err = vfs_sync();
	if(err){
		*ret = -1;
		return err;
	}	

	*ret = 0;
	return 0;
}
