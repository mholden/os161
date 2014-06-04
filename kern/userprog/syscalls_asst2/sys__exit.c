#include <types.h>
#include <syscall.h>
#include <thread.h>
#include <pcb_list.h>
#include <synch.h>
#include <curthread.h>

/* 
 * The exit() syscall.
 */

void sys__exit(int exit_code){
	
	lock_acquire(pcb_list_lock);
	struct pcb_node *curr_node = pcb_list_root;
	
	while(curr_node != NULL){
		if(curr_node->thread_id == curthread) break;
		curr_node = curr_node->next;
	}
	
	curr_node->exited = 1;
	curr_node->exit_code = exit_code;
	cv_signal(curr_node->exit_cv, pcb_list_lock);
	
	lock_release(pcb_list_lock);

	thread_exit();
}
