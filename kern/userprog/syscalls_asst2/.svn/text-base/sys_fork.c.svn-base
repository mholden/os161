#include <types.h>
#include <syscall.h>
#include <machine/trapframe.h>
#include <pcb_list.h>
#include <lib.h>
#include <synch.h>

/* 
 * The fork() syscall.
 */

void md_forkentry(struct trapframe *tf, pid_t child_pid){
	(void) child_pid; // turns out we didn't need it.. I'll clean this up later

	lock_acquire(pcb_fork_lock); // for synchronization.. pretty ugly but whatever
	lock_release(pcb_fork_lock);

	/* Make a local copy of tf for our stack, and initialize it */
	struct trapframe tf_for_stack = *tf;
	tf_for_stack.tf_v0 = 0;
	tf_for_stack.tf_a3 = 0;
	tf_for_stack.tf_epc += 4;

	/* 
	 * Inheriting the parent's addrspace has been added in thread_fork(),
	 * so at this point we have an address space the same as our parent's,
	 * it's been activated in mi_threadstart(), and we're ready
	 * to enter usermode.
	 */

	mips_usermode(&tf_for_stack);
}

pid_t sys_fork(struct trapframe *tf){
	
	// Acquire a pid	
	pid_t child_pid = acquire_pid();
	if(child_pid == -1){
		return -1;
	}

	/*
	 * Deep copy tf, to be safe. thread_fork() only puts our new thread
	 * on the runqueue. Since our new thread needs data in tf, but it is
	 * not gauranteed that our new thread will run before our current thread
	 * (this one) messes around with the stack (and therefore messes around
	 * with tf), we better deep copy it, so that no one else can mess with
	 * it.
	 */
	struct trapframe *tf_copy = kmalloc(sizeof(struct trapframe));
	if(tf_copy == NULL){
		free_pid(child_pid);
		return -2;
	}
	*tf_copy = *tf;

	char name[16];
	snprintf(name, sizeof(name), "process%d", child_pid);
	
	/* 
	 * Fork the new thread. I created another lock for this section of code
	 * so that the new thread/process can't do a getpid() (or something like 
	 * that) before I have even created the pcb node.
	 */
	struct thread *new_thread;
	lock_acquire(pcb_fork_lock);
	int result = thread_fork(name, tf_copy, child_pid, (void (*)(void *, unsigned long)) md_forkentry, &new_thread);
	if(result){
		lock_release(pcb_fork_lock);
		free_pid(child_pid);
		return -2;
	}
	// Insert the new pcb node
	struct pcb_node *new_node = insert_pcb_node(child_pid, new_thread);
	if(new_node == NULL){
		lock_release(pcb_fork_lock);
		free_pid(child_pid);
 		return -2;
	}
	lock_release(pcb_fork_lock);
	
	return child_pid;
}
