#include <types.h>
#include <lib.h>
#include <vnode.h>
#include <fd.h>
#include <curthread.h>
#include <kern/errno.h>
#include <thread.h>

int acquire_fd(struct vnode* file){
	
	/* Should probably error check file */
	
	int i; 
	for(i=0; i < MAX_FILES_PER_THREAD; i++){
		if(curthread->file_descriptors[i] == NULL) break;
	}
	if(i == MAX_FILES_PER_THREAD) return ENOSPC;
	
	curthread->file_descriptors[i] = kmalloc(sizeof(struct fd));
	if(curthread->file_descriptors[i] == NULL) return ENOMEM;
	
	curthread->file_descriptors[i]->file = file;
	curthread->file_descriptors[i]->curr_offset = 0;
	return i;
}

void release_fd(int filehandle){
	kfree(curthread->file_descriptors[filehandle]);
	curthread->file_descriptors[filehandle] = NULL;
}
