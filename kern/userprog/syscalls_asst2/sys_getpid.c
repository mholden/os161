#include <types.h>
#include <lib.h>
#include <syscall.h>
#include <synch.h>
#include <pcb_list.h>
#include <curthread.h>

/* 
 * The getpid() syscall.
 */

pid_t sys_getpid(void){
	pid_t ret = -1; // will return -1 if pid is somehow not found - this should never happen
	lock_acquire(pcb_list_lock);
	struct pcb_node *curr_node = pcb_list_root;
	while(curr_node != NULL){
		if(curr_node->thread_id == curthread){
			ret = curr_node->pid;
			break;
		}
		curr_node = curr_node->next;
	}
	lock_release(pcb_list_lock);
	
	return ret;
}
