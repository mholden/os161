#ifndef _PCB_LIST_H_
#define _PCB_LIST_H_

#include <thread.h>
#include <synch.h>
#include <kern/types.h>

/*
 * Definition of a pcb_node, and our pcb_list functions. These structures
 * and functions are used to keep track of our current processes (intended to 
 * be used as a simple linked list). There is already a struct pcb defined
 * in mips/pcb.h, but that seemed more like a tcb (thread control block), 
 * so I decided to define a pcb_list for processes.
 */

#define MAX_NUM_PROCESSES 50 // 50 seems reasonable, but change as needed

struct pcb_node {
	struct thread *thread_id;
	pid_t pid;
	int exited;
	struct cv *exit_cv;
	int exit_code;
	struct pcb_node *next;
};

extern struct pcb_node *pcb_list_root;
extern int pid_avail[MAX_NUM_PROCESSES];
extern struct lock *pcb_list_lock;
extern struct lock *pcb_fork_lock;
extern struct lock *pid_avail_lock;

/* Acquire a pid if one is available. Returns an available pid on success, -1 if none are available */
pid_t acquire_pid(void);

/* Create and insert a new pcb_node with pid and thread_id into our pcb list */
struct pcb_node* insert_pcb_node(pid_t pid, struct thread *thread_id);

/* Free pid for reuse */
void free_pid(pid_t pid);

/* Delete the node_to_destroy from our pcb list */
void destroy_node(struct pcb_node *node_to_destroy);

#endif /* _PCB_LIST_H_ */
