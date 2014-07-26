#ifndef _FD_H_
#define _FD_H_

#include <kern/types.h>
#include <pcb_list.h>

struct vnode;
struct lock;

/*
 * Definition of our file descriptor structures, variables,
 * and functions.
 *
 * Here's how it works:
 * 1. Each process/thread has its own file table, an array of 
 *    file descriptors (struct fds).
 * 2. Files can be shared between processes though, which is
 *    why we have a system-wide file table, too (an array of 
 *    struct file_nodes). This allows for our file locks to 
 *    actually work (unlike other implementations of the file
 *    locks seen on the Pearls blog and github).
 * 3. vn->opencount is only ever incremented in vfs_open, so 
 *    we don't want to decrement it unless the process actually
 *    vfs_opened it, so every time vfs_open is called on a file
 *    we set the opened flag in struct fd. when closing/cleaning
 *    up files, only actually decrement vn->opencount (by
 *    calling vfs_close) if opened == 1. there are many 
 *    situations in which a process will have a file in its
 *    file table and not have actually vfs_opened it (after
 *    a fork, for example). dup2 shares struct fds, though,
 *    which complicates things even further. Not only do we 
 *    need to check that opened == 1, but also that ref_count
 *    of the struct fd == 1 (logic in close_file() is correct). 
 * 4. vn->refcount and ref_count are incremented any time a 
 *    process starts referencing the file, even if the process 
 *    did not vfs_open it. 
 * 5. ref_count in the system wide file table keeps track of
 *    userland references, and is used to clean up/remove
 *    file_nodes when it hits 0. vn->refcount keeps track of
 *    both userland references AND any other references within
 *    the system (ie. if(vn->refcount == 0) then clean up 
 *    file_node wouldn't work like we want it to because the 
 *    system may have references to the vnode even when all 
 *    userland references are gone). vn->refcount >= ref_count
 * 6. To confuse things even further, I've added a ref_count to 
 *    struct fd for use with dup2.. so this ref_count refers
 *    to the number of file descriptor pointers in 
 *    curthread->file_descriptors that are pointing to this
 *    particular struct fd. 
 */

#define MAX_FILES_PER_THREAD 20

struct fd{
	struct vnode *file;
	char *filename;
	off_t curr_offset;
	int rw_flags;
	int opened;
	int ref_count;
};


struct file_node{
	struct vnode *file;
	struct lock *lock;
	int ref_count;
};


/* System-wide file table and its lock */
extern struct file_node *file_table[MAX_NUM_PROCESSES * MAX_FILES_PER_THREAD];
extern struct lock *file_table_lock;

/* Acquire a file descriptor for the current thread */
int acquire_fd(int *fd);

/* Initialize a file descriptor */
int init_fd(int filehandle, struct vnode *file, const char *filename, off_t offset, int flags, int opened_flag);

/* Duplicate a file descriptor (to be used in sys_dup2) */
int dup_fd(int filehandle, int newhandle);

/* Release/free a file descriptor */
void release_fd(int filehandle);

/* Do all the work for sys_close */
void close_file(int filehandle);

/* If file does not exist in file table, add it. Otherwise increment ref count */
int check_to_add_file_node(struct vnode *file, const char *filename);

/* If ref count == 1, remove file from file table. Otherwise decrement ref count */
void check_to_remove_file_node(struct vnode *file);

/* Acquire the lock associated with file */
int acquire_file_lock(struct vnode *file);

/* Release the lock associated with file */
void release_file_lock(struct vnode *file);

#endif /* _FD_H_ */
