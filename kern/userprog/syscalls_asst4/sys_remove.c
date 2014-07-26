#include <types.h>
#include <lib.h>
#include <kern/errno.h>
#include <syscall.h>
#include <vfs.h>

/*
 *  The remove() syscall.
 */

int sys_remove(const char *filename, int *ret){
	int err;
	char *filename_cp;

	/* Error check inputs */
	if((filename == NULL) || (ret == NULL)){
		*ret = -1;
		return EINVAL;
	}

	filename_cp = kmalloc(strlen(filename) + 1);
	if(filename_cp == NULL){
		*ret = -1;
		return ENOMEM;
	}
	strcpy(filename_cp, filename);

	err = vfs_remove(filename_cp);
	kfree(filename_cp);
	if(err){
                *ret = -1;
                return err;
        }

	*ret = 0;
	return 0;
}
