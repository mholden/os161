#include <types.h>
#include <syscall.h>
#include <machine/trapframe.h>
#include <pcb_list.h>
#include <lib.h>
#include <synch.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <addrspace.h>
#include <curthread.h>
#include <vfs.h>
#include <vm.h>

/*
 * The execv syscall
 *
 */

pid_t
sys_execv(struct trapframe *tf)
{
	char *prog;
	char **args;
	prog = (char *) tf->tf_a0;
	args = (char **) tf->tf_a1;

	/*
	 * Copying arguments into kernel buffer
	 * first checking how much space is needed for buffer
	 * then the strings are copied into kbuff using copyin
	 */
	char **usr_point = kmalloc(4);	//pointer to a char pointer from *argv[] array
	if (usr_point == NULL)
		return ENOMEM;
	char *str_point;
	int err = 0;	//Error code
	int argc = 0;		//index when going through arguments in args
	int str_index;	//index when going through strings in args
	size_t count_bytes = 0;		/*Used to keep track of how many bytes are needed for kbuf*/
	while (args[argc] != NULL)		//The end of argc is signaled by a NULL pointer
	{
		if((err = copyin((const_userptr_t) &args[argc], (void *)usr_point, 4)) != 0)	//copy the pointer that
			return err;														//should be in user space

		str_point = *usr_point;
		for(str_index = 0; str_point[str_index] != '\0'; str_index++)	//to make sure it is '\0' ended
		{
			if (str_index >= MAX_STRLENGHT_EXECV)
				return E2BIG;
		}
		str_index++;	//take into account null char
		if (str_index % 4 == 0)				/*This part adds to the number of bytes rounded by 4*/
			count_bytes += str_index;
		else
			count_bytes += (str_index / 4 + 1) * 4;
		if (argc >= MAX_PARAMS_EXECV)
			return E2BIG;
		count_bytes += 4;
		argc++;
	}
	count_bytes += 4;		//4 more bytes for the NULL pointer
	char **kbuf = kmalloc(count_bytes);		//kernel buffer
	if (kbuf == NULL)
		return ENOMEM;
	char *pos_at_kbuf;
	char *temp;
	int i = 0;
	int length_str = 0;
	int base_str = (argc + 1) * 4;		//offset for the values of the pointers
	while (i < argc)
	{
		kbuf[i] = (void *)base_str;
		temp = args[i];		//the string in user space, no need to check it since was done before
		length_str = strlen(temp) + 1;		//the length of the string including null char
		pos_at_kbuf = (char *)&kbuf[base_str / 4];		//address to copy to in buffer
		if((err = copyin((const_userptr_t) temp, (void *)pos_at_kbuf, length_str)) != 0)
			return err;
		while ((length_str % 4 != 0))	//filling with null chars if string is not rounded to 4
		{
			pos_at_kbuf[length_str] = '\0';
			length_str++;
		}
		base_str += length_str;			//updating the offset for the next pointer
		i++;		//index
	}
	kbuf[argc] = (void *)NULL;		//set the final NULL pointer

	/* Now we need to copy the prog pointer into a kernel buffer too */

	/*
	 * Open executable return error if unvalid
	 * destroy current address space since it is the parent's
	 * create a new one and activate it
	 */

	vaddr_t entrypoint, stackptr;
	struct vnode *v;

	/* Try.. */
	struct addrspace *new_as = as_create(prog);

	/* Destroy the current address space. */
	as_destroy(curthread->t_vmspace);

	/* Create a new address space. */
	curthread->t_vmspace = new_as;//as_create(prog);
	if (curthread->t_vmspace==NULL) {
		//vfs_close(v);
		return ENOMEM;
	}
	/* Activate it. */
	as_activate(curthread->t_vmspace);

	/* Open the file. */
	err = vfs_open(curthread->t_vmspace->progname/*prog*/, O_RDONLY, &v);
	if (err) {
			return err;
	}

	/* Load the executable. */
	err = load_elf(v, &entrypoint);
	if (err) {
		/* thread_exit destroys curthread->t_vmspace */
		vfs_close(v);
		return err;
	}

	/* Done with the file now. */
	vfs_close(v);

	/* Define the user stack in the address space */
	err = as_define_stack(curthread->t_vmspace, &stackptr);
	if (err) {
		/* thread_exit destroys curthread->t_vmspace */
		return err;
	}

	/*
	 * Copy the arguments into user stack
	 */
	i = 0;
	while (i < argc)
	{
		kbuf[i] = (void *)(stackptr - count_bytes + (int)kbuf[i]);
		i++;
	}
	err = copyout((void *) kbuf, (userptr_t) (stackptr - count_bytes), (size_t) count_bytes);
	if (err)
		return err;

	/* Warp to user mode. */
	md_usermode(argc /*argc*/, (userptr_t) (stackptr - count_bytes) /*userspace addr of argv*/,
			(vaddr_t) (stackptr - count_bytes), entrypoint);

	/* md_usermode does not return */
	panic("md_usermode returned\n");
	return EINVAL;
}
