#include <types.h>
#include <lib.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <syscall.h>
#include <vfs.h>
#include <vnode.h>
#include <fd.h>
#include <curthread.h>
#include <thread.h>
#include <synch.h>

/*
 *  The open() syscall.
 */

int sys_open(const char *filename, int flags, int *ret){
	int err, fd;
	char *filename_cp;
	struct vnode *file;

	/* Error check inputs */
	if(filename == NULL) return EINVAL;
	assert(ret != NULL);

	/* vfs_open destroys filename, so copy it elsewhere (we need to use it again below) */
	filename_cp = kmalloc(strlen(filename) + 1);
	if(filename_cp == NULL) return ENOMEM;
	strcpy(filename_cp, filename);

	err = vfs_open(filename_cp, flags, &file);
	kfree(filename_cp);
	if(err) return err;

	err = check_to_add_file_node(file, filename); 
	if(err){
		vfs_close(file);
		return err;
	}

	err = acquire_fd(&fd);
        if(err){
		check_to_remove_file_node(file);
                vfs_close(file);
                return err;
        }

	err = init_fd(fd, file, filename, 0, flags & O_ACCMODE, 1);
	if(err){	
		check_to_remove_file_node(file);	
		vfs_close(file);
		return err;
	}

	*ret = fd;

	return 0;
}
