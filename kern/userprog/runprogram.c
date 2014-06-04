/*
 * Sample/test code for running a user program.  You can use this for
 * reference when implementing the execv() system call. Remember though
 * that execv() needs to do more than this function does.
 */

#include <types.h>
#include <kern/unistd.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <thread.h>
#include <curthread.h>
#include <vm.h>
#include <vfs.h>
#include <test.h>

/*
 * Load program "progname" and start running it in usermode.
 * Does not return except on error.
 *
 * Calls vfs_open on progname and thus may destroy it.
 */
int
runprogram(char *progname, char **args, int argc)
{
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;

	/*
	 * Handle the args
	 */
	char *temp;
	int i = 0;
	int str_len;
	size_t count_bytes = 0;		/*Used to keep track of how many bytes are needed for kbuf*/
	while(i < argc)
	{
		temp = args[i];
		str_len = strlen(temp) + 1;
		if(str_len % 4 == 0)
			count_bytes += str_len;
		else
			count_bytes += ((str_len / 4) + 1) * 4;
		count_bytes += 4;
		i++;
	}
	count_bytes += 4;
	char **kbuf = kmalloc (count_bytes);
	if (kbuf == NULL)
		return ENOMEM;
	i = 0;
	char *pos_at_kbuf;
	int base_str = (argc + 1) * 4;
	while(i < argc)
	{
		kbuf[i] = (void *) base_str;
		temp = args[i];
		pos_at_kbuf = (char *)&kbuf[base_str / 4];		//address to copy to in buffer
		strcpy(pos_at_kbuf, temp);
		str_len = strlen(temp) + 1;		//the length of the string including null char
		while ((str_len % 4 != 0))	//filling with null chars if string is not rounded to 4
		{
			pos_at_kbuf[str_len] = '\0';
			str_len++;
		}
		base_str += str_len;			//updating the offset for the next pointer
		i++;		//index
	}

	kbuf[argc] = (void *)NULL;		//set the final NULL pointer

	//Done

	/* We should be a new thread. */
	assert(curthread->t_vmspace == NULL);

	//kprintf("We're in runprogram, and evidently have not yet called our alloc_npages().\n");//TEST

	/* Create a new address space. */
	curthread->t_vmspace = as_create(progname);

	if (curthread->t_vmspace==NULL) {
		return ENOMEM;
	}

	/* Activate it. */
	as_activate(curthread->t_vmspace);

	/* Open the file. */
	result = vfs_open(progname, O_RDONLY, &v);
	if (result) {
		return result;
	}

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		/* thread_exit destroys curthread->t_vmspace */
		vfs_close(v);
		return result;
	}

	/* Done with the file now. */
	vfs_close(v);

	//kprintf("Safely out of load_elf().\n");//TEST
	
	/* Define the user stack in the address space */
	result = as_define_stack(curthread->t_vmspace, &stackptr);
	if (result) {
		/* thread_exit destroys curthread->t_vmspace */
		return result;
	}

	/*
	 * Alright, let the real coding begin. We're gunna page fault
	 * below, almost immediately. Our vm system needs to be working
	 * for the code below to work.
 	 */

	/* Copy the arguments into user stack */
	i = 0;
	while (i < argc)
	{
		kbuf[i] = (void *)(stackptr - count_bytes + (int)kbuf[i]);
		i++;
	}
	result = copyout((void *) kbuf, (userptr_t) (stackptr - count_bytes), (size_t) count_bytes);
	if (result)
		return result;

	/* Warp to user mode. */
	md_usermode(argc /*argc*/, (userptr_t) (stackptr - count_bytes) /*userspace addr of argv*/,
				(vaddr_t) (stackptr - count_bytes), entrypoint);
	
	/* md_usermode does not return */
	panic("md_usermode returned\n");
	return EINVAL;
}

