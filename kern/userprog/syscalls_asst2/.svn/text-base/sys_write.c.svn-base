#include <types.h>
#include <lib.h>
#include <syscall.h>

/* 
 * The write() syscall.
 */

int sys_write(int filehandle, const void *buf, size_t size){
	// For now just keep it simple.
	(void) filehandle;
	
	char *to_print;
	
	if(size == 1){ // we're getting called from putchar
		to_print = kmalloc(2);
		to_print[0] = ((char *)buf)[0];
		to_print[1] = '\0';
	}
	// assumes that if we're not being called from putchar(), buf will be null terminated. pretty hack..
	else to_print = (char *)buf;

	kprintf(to_print);
	if(size == 1) kfree(to_print);
	return 0; // Assume success.
}
