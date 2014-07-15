#include <types.h>
#include <lib.h>
#include <vnode.h>
#include <fd.h>
#include <curthread.h>
#include <kern/errno.h>
#include <kern/types.h>
#include <thread.h>
#include <synch.h>
#include <pcb_list.h>
#include <vfs.h>

struct file_node *file_table[MAX_NUM_PROCESSES * MAX_FILES_PER_THREAD];
struct lock *file_table_lock;

int init_fd(int filehandle, struct vnode *file, const char *filename, off_t offset, int flags, int opened_flag){
	struct fd *fd;
	
	assert(filehandle >= 0);
	assert(file != NULL);
	assert(filename != NULL);

	fd = kmalloc(sizeof(struct fd));
	if(fd == NULL) return ENOMEM;

	fd->file = file;
	
	fd->filename = kstrdup(filename);
	if(fd->filename == NULL){
		kfree(fd);
		return ENOMEM;	
	}

	fd->curr_offset = offset;
	fd->rw_flags = flags;
	fd->opened = opened_flag;

	curthread->file_descriptors[filehandle] = fd;

	return 0;
}

int acquire_fd(int *fd){
	int i;

	assert(fd != NULL);
 
	for(i=0; i < MAX_FILES_PER_THREAD; i++){
		if(curthread->file_descriptors[i] == NULL) break;
	}
	if(i == MAX_FILES_PER_THREAD) return ENOSPC;	

	*fd = i;
	return 0;
}

void release_fd(int filehandle){
	assert(filehandle >= 0);
	assert(curthread->file_descriptors[filehandle] != NULL);

	kfree(curthread->file_descriptors[filehandle]->filename);
	kfree(curthread->file_descriptors[filehandle]);
	curthread->file_descriptors[filehandle] = NULL;
	
	return;
}

void close_file(int filehandle){
	/* Assumes filehandle has already been error checked */

	int opened, *ref_count;
        struct vnode *file;

        file = curthread->file_descriptors[filehandle]->file;
        opened = curthread->file_descriptors[filehandle]->opened;

        release_fd(filehandle);
	dec_refcount(file);
        get_file_refcount(file, &ref_count);
        if(*ref_count == 0) remove_file_node(file);
        if(!opened) VOP_DECREF(file);
        else vfs_close(file);	
}

int add_file_node(struct vnode *file, struct lock *lock, int *ref_count){
	int i;

	assert(file != NULL);
	assert(lock != NULL);
	assert(ref_count != NULL);

	lock_acquire(file_table_lock);
	
	for(i = 0; i < MAX_NUM_PROCESSES * MAX_FILES_PER_THREAD; i++){
		if(file_table[i] == NULL) break;
		// Sanity check.. there should never be duplicate entries 
		assert(file_table[i]->file != file); 
	}

	if(i == MAX_NUM_PROCESSES * MAX_FILES_PER_THREAD){
		lock_release(file_table_lock);
		return ENOSPC;
	}

	file_table[i] = kmalloc(sizeof(struct file_node));
	if(file_table[i] == NULL){
		lock_release(file_table_lock);
		return ENOMEM;
	}

	file_table[i]->file = file;
	file_table[i]->lock = lock;
	file_table[i]->ref_count = ref_count;

	lock_release(file_table_lock);
	
	return 0;
}

void remove_file_node(struct vnode *file){
	int i;

        assert(file != NULL);

        lock_acquire(file_table_lock);

        for(i = 0; i < MAX_NUM_PROCESSES * MAX_FILES_PER_THREAD; i++){
                if(file_table[i] == NULL) continue;
		if(file_table[i]->file == file) break;
        }

        if(i == MAX_NUM_PROCESSES * MAX_FILES_PER_THREAD){
                /* file wasn't even there? do nothing.. */
		lock_release(file_table_lock);
                return;
        }

        lock_destroy(file_table[i]->lock);
        kfree(file_table[i]->ref_count);
	kfree(file_table[i]);
	file_table[i] = NULL;

        lock_release(file_table_lock);

        return;
}

int get_file_lock(struct vnode *file, struct lock **ret){
	int i;

	assert(file != NULL);
        assert(ret != NULL);

        lock_acquire(file_table_lock);

        for(i = 0; i < MAX_NUM_PROCESSES * MAX_FILES_PER_THREAD; i++){
                if(file_table[i] == NULL) continue;
		if(file_table[i]->file == file) break;
        }

        if(i == MAX_NUM_PROCESSES * MAX_FILES_PER_THREAD){
                lock_release(file_table_lock);
                return EINVAL;
        }

	*ret = file_table[i]->lock;
	
	lock_release(file_table_lock);

	return 0;
}

int get_file_refcount(struct vnode *file, int **ret){
	int i;

        assert(file != NULL);
        assert(ret != NULL);

        lock_acquire(file_table_lock);

        for(i = 0; i < MAX_NUM_PROCESSES * MAX_FILES_PER_THREAD; i++){
                if(file_table[i] == NULL) continue;
                if(file_table[i]->file == file) break;
        }

        if(i == MAX_NUM_PROCESSES * MAX_FILES_PER_THREAD){
                lock_release(file_table_lock);
                return EINVAL;
        }

        *ret = file_table[i]->ref_count;

        lock_release(file_table_lock);

        return 0;
}

int file_node_exists(struct vnode *file){
	int i;

        assert(file != NULL);

        lock_acquire(file_table_lock);

        for(i = 0; i < MAX_NUM_PROCESSES * MAX_FILES_PER_THREAD; i++){
                if(file_table[i] == NULL) continue;
                if(file_table[i]->file == file) break;
        }

	lock_release(file_table_lock);

        if(i == MAX_NUM_PROCESSES * MAX_FILES_PER_THREAD) return 0;
	else return 1;
}

int inc_refcount(struct vnode *file){
	int i;

        assert(file != NULL);

        lock_acquire(file_table_lock);

        for(i = 0; i < MAX_NUM_PROCESSES * MAX_FILES_PER_THREAD; i++){
                if(file_table[i] == NULL) continue;
                if(file_table[i]->file == file) break;
        }

	if(i == MAX_NUM_PROCESSES * MAX_FILES_PER_THREAD){
                lock_release(file_table_lock);
                return EINVAL;
        }

	*(file_table[i]->ref_count) += 1;

        lock_release(file_table_lock);

        return 0;
}

int dec_refcount(struct vnode *file){
	int i;
        
        assert(file != NULL);
        
        lock_acquire(file_table_lock);
        
        for(i = 0; i < MAX_NUM_PROCESSES * MAX_FILES_PER_THREAD; i++){
                if(file_table[i] == NULL) continue;
                if(file_table[i]->file == file) break;
        }       

        if(i == MAX_NUM_PROCESSES * MAX_FILES_PER_THREAD){
                lock_release(file_table_lock);
                return EINVAL;
        }
        
        *(file_table[i]->ref_count) -= 1;
        
        lock_release(file_table_lock);

        return 0;
}
