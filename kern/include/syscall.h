#ifndef _SYSCALL_H_
#define _SYSCALL_H_

#include <kern/types.h>
#include <machine/trapframe.h>

/*
 * Max number of paramenters for passing into a new process through execv
 * Otherwise, would have to keep scanning array of pointer until a NULL reaches
 */
#define	MAX_PARAMS_EXECV	10
#define MAX_STRLENGHT_EXECV	200

/*
 * Prototypes for IN-KERNEL entry points for system call implementations.
 */


int sys_reboot(int code);
int sys_write(int filehandle, const void *buf, size_t size);
void sys__exit(int exit_code);
pid_t sys_getpid(void);
pid_t sys_fork(struct trapframe *tf);
pid_t sys_execv(struct trapframe *tf);
int sys_waitpid(pid_t pid, int *returncode, int options);
int sys_read(int filehandle, void *buf, size_t size);
void* sys_sbrk(intptr_t amount);
int sys_open(const char *filename, int flags);
int sys_close(int fd);
#endif /* _SYSCALL_H_ */
