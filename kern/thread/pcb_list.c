#include <types.h>
#include <lib.h>
#include <synch.h>
#include <curthread.h>
#include <pcb_list.h>

/*
 * pcb_list related functions and variables. Most variables
 * are initialized in thread_bootstrap().
 */

/* Global variable for the root of our pcb_list. */
struct pcb_node *pcb_list_root;

/* Global array to keep track of available pids */
int pid_avail[MAX_NUM_PROCESSES];

/* Global locks */
struct lock *pcb_list_lock;
struct lock *pcb_fork_lock;
struct lock *pid_avail_lock;

pid_t acquire_pid(void){
	int i;
	lock_acquire(pid_avail_lock);
	for(i = 0; i < MAX_NUM_PROCESSES; i++){
		if(pid_avail[i]){
			pid_avail[i] = 0;
			break;
		}
	}
	lock_release(pid_avail_lock);
	if(i == MAX_NUM_PROCESSES) return -1;
	else return i + 1; // never want pid of 0
}


struct pcb_node* insert_pcb_node(pid_t pid, struct thread *thread_id){
	
	// create and initialize a new node
	struct pcb_node *new_node = kmalloc(sizeof(struct pcb_node));
	if(new_node == NULL) return NULL;
	new_node->thread_id = thread_id;
	new_node->pid = pid;
	new_node->exited = 0;

	char name[16];
	snprintf(name, sizeof(name), "exit_cv%d", pid);

	new_node->exit_cv = cv_create(name);
	if(new_node->exit_cv == NULL) return NULL;
	new_node->exit_code = 0;
	
	// insert at head of list
	lock_acquire(pcb_list_lock);
	new_node->next = pcb_list_root;
	pcb_list_root = new_node;
	lock_release(pcb_list_lock);
	
	return new_node;
}

void free_pid(pid_t pid){
	// shouldn't need the lock here, just a single write - no automicity required
	assert(pid_avail[pid - 1] == 0);
	pid_avail[pid - 1] = 1;
}

void destroy_node(struct pcb_node *node_to_destroy){
	// pcb_list_lock acquired before entering this function, so don't acquire it here
	struct pcb_node *prev_node = NULL;
	struct pcb_node *curr_node = pcb_list_root;

	while(curr_node != NULL){
		if(curr_node == node_to_destroy) break;
		prev_node = curr_node;
		curr_node = curr_node->next;
	}

	if(prev_node == NULL) pcb_list_root = curr_node->next;
	else prev_node->next = curr_node->next;
	curr_node->next = NULL;
	cv_destroy(curr_node->exit_cv);
	kfree(node_to_destroy);
}






