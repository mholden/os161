#include <types.h>
#include <syscall.h>
#include <lib.h>
#include <kern/errno.h>
#include <vfs.h>

/*
 * The mkdir() syscall.
 */

int sys_mkdir(const char *dirname, int ignore, int *ret){
	(void)ignore;
	int err;
	char *dirname_cp;

	if(ret == NULL) return EINVAL;
	if(dirname == NULL){
		*ret = -1;
		return EINVAL;
	}

	dirname_cp = kmalloc(strlen(dirname) + 1);
	if(dirname_cp == NULL){
		*ret = -1;
		return ENOMEM;
	}

	strcpy(dirname_cp, dirname);

	err = vfs_mkdir(dirname_cp);
	kfree(dirname_cp);
	if(err){
		*ret = -1;
		return err;
	}

	*ret = 0;
	return 0;
}
