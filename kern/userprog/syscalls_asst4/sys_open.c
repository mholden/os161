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
	char *filename_cp, *lock_name;
	struct vnode *file; 
	struct lock *lock;
	int *ref_count;

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

	err = acquire_fd(&fd);
	if(err){ 
		vfs_close(file);
		return err;
	}
	
	if(file_node_exists(file)){
		/* vnode is already being used.. find its lock */
		err = get_file_lock(file, &lock);
		if(err){
			vfs_close(file);
			return err;
		}
	} else{
		/* vnode isn't being used by other processes (yet), create a file lock for it */
		lock_name = kmalloc(strlen(filename) + 6);
        	if(lock_name == NULL){
                        vfs_close(file);
			return ENOMEM;
		}
        	strcpy(lock_name, filename);
        	strcat(lock_name, "_lock");
        	lock = lock_create(lock_name);
		kfree(lock_name);
        	if(lock == NULL){
                        vfs_close(file);
			return ENOMEM;
        	}

		/* Also create and initialize its ref_count */
		ref_count = kmalloc(sizeof(int));
		if(ref_count == NULL){
			lock_destroy(lock);
			vfs_close(file);
                        return ENOMEM;
		}
		*ref_count = 1;
	
		/* Add the file to our file table */
		err = add_file_node(file, lock, ref_count);
		if(err){
			kfree(ref_count);
                        lock_destroy(lock);
			vfs_close(file);
                        return err;
		}	
	}

	err = init_fd(fd, file, filename, 0, flags & O_ACCMODE, 1);
	if(err){	
		get_file_refcount(file, &ref_count);
		if(*ref_count == 1) remove_file_node(file);
		vfs_close(file);
		return err;
	}

	*ret = fd;

	return 0;
}
