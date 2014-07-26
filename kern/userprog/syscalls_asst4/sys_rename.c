#include <types.h>
#include <syscall.h>
#include <kern/errno.h>
#include <lib.h>
#include <vfs.h>

/*
 * The rename() syscall.
 */

int sys_rename(const char *oldfile, const char *newfile, int *ret){
	int err;
        char *oldfile_cp, *newfile_cp;

        if(ret == NULL) return EINVAL;
        if(oldfile == NULL || newfile == NULL){
                *ret = -1;
                return EINVAL;
        }

        oldfile_cp = kmalloc(strlen(oldfile) + 1);
        if(oldfile_cp == NULL){
                *ret = -1;
                return ENOMEM;
        }

	strcpy(oldfile_cp, oldfile);

	newfile_cp = kmalloc(strlen(newfile) + 1);
        if(newfile_cp == NULL){
                *ret = -1;
                return ENOMEM;
        }

	strcpy(newfile_cp, newfile);

        err = vfs_rename(oldfile_cp, newfile_cp);
        kfree(oldfile_cp);
	kfree(newfile_cp);
        if(err){
                *ret = -1;
                return err;
        }

	/* Should I rename filename in struct fd and lockname in file_table?? */

        *ret = 0;
        return 0;
}
