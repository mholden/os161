#ifndef _FD_H_
#define _FD_H_

#include <vnode.h>
#include <kern/types.h>

/*
 * Definition of our file descriptor structure, and variables
 * and functions associated with it.
 */

#define MAX_FILES_PER_THREAD 20

struct fd {
	struct vnode* file;
	off_t curr_offset;
};

/* Acquire a file descriptor for the current thread and the given file */
int acquire_fd(struct vnode* file);

/* Release/free a file descriptor */
void release_fd(int filehandle);

#endif /* _FD_H_ */
