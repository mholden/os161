#include <types.h>
#include <lib.h>
#include <syscall.h>
#include <synch.h>
#include <pcb_list.h>
#include <curthread.h>
#include <kern/errno.h>

/* 
 * The waitpid() syscall.
 */

int sys_waitpid(pid_t pid, int *returncode, int options, int *ret){
	if(options != 0){
		*ret = -1;
		return EINVAL;
	}
	if(returncode == NULL){
		*ret = -1;
                return EINVAL;
	}
	
	lock_acquire(pcb_list_lock);
	struct pcb_node *curr_node = pcb_list_root;
	while(curr_node != NULL){
		if(curr_node->pid == pid) break;
		curr_node = curr_node->next;
	}
	if(curr_node == NULL){
		*ret = -1;
                return EINVAL;
        }

	// wait
	while(curr_node->exited == 0) cv_wait(curr_node->exit_cv, pcb_list_lock);
	*returncode = curr_node->exit_code;
	
	// free the pcb and make pid usable again
	pid_avail[curr_node->pid - 1] = 1;
	destroy_node(curr_node);
	lock_release(pcb_list_lock);

	*ret = pid;
	return 0;
}
