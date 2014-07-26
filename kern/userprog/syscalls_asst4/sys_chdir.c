#include <types.h>
#include <syscall.h>
#include <lib.h>
#include <kern/errno.h>
#include <vfs.h>

/*
 * The chdir() syscall.
 */

int sys_chdir(const char *path, int *ret){
	int err;
	char *path_cp;

	if(ret == NULL) return EINVAL;
        if(path == NULL){
                *ret = -1;
                return EINVAL;
        }

	path_cp = kmalloc(strlen(path) + 1);
	if(path_cp == NULL){
		*ret = -1;
		return ENOMEM;
	}

	strcpy(path_cp, path);

	err = vfs_chdir(path_cp);
	kfree(path_cp);
	if(err){
		*ret = -1;
		return err;
	}	

	*ret = 0;
	return 0;
}
