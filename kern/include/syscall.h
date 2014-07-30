#ifndef _SYSCALL_H_
#define _SYSCALL_H_

#include <kern/types.h>
#include <machine/trapframe.h>
#include <kern/stat.h>

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
int sys_write(int filehandle, const void *buf, size_t size, int *ret);
void sys__exit(int exit_code);
pid_t sys_getpid(void);
pid_t sys_fork(struct trapframe *tf);
pid_t sys_execv(struct trapframe *tf);
int sys_waitpid(pid_t pid, int *returncode, int options, int *ret);
int sys_read(int filehandle, void *buf, size_t size, int *ret);
void* sys_sbrk(intptr_t amount);
int sys_open(const char *filename, int flags, int *ret);
int sys_close(int filehandle);
int sys_fstat(int fd, struct stat *buf);
off_t sys_lseek(int filehandle, off_t pos, int code, int *ret);
int sys_remove(const char *filename, int *ret);
int sys_chdir(const char *path, int *ret);
int sys___getcwd(char *buf, size_t buflen, int *ret);
int sys_mkdir(const char *dirname, int ignore, int *ret);
int sys_rmdir(const char *dirname, int *ret);
int sys_rename(const char *oldfile, const char *newfile, int *ret);
int sys_sync(int *ret);
int sys_fsync(int filehandle, int *ret);
int sys_getdirentry(int filehandle, char *buf, size_t buflen, int *ret);
int sys_dup2(int filehandle, int newhandle, int *ret);

#endif /* _SYSCALL_H_ */
