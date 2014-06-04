/*
 * simple_forktest - test fork()
 */

#include <unistd.h>
#include <sys/types.h>

int
main(int argc, char *argv[])
{
	(void) argc;
	(void) argv;

	pid_t pid = fork();
	if(pid == 0){
		write(0, "Printing from the child process!\n", 0);
		pid = getpid();
		switch(pid){
			case 1: 
				write(0, "child pid is 1 (from child)\n", 0);
				break;
			case 2:
				write(0, "child pid is 2 (from child)\n", 0);
				break;
			case 3:
				write(0, "child pid is 3 (from child)\n", 0);
				break;
			case 4:
				write(0, "child pid is 4 (from child)\n", 0);
				break;
			case 5:
				write(0, "child pid is 5 (from child)\n", 0);
				break;
			case 6:
				write(0, "child pid is 6 (from child)\n", 0);
				break;
			default:
				write(0, "child pid is not between 1 and 6 (from child)\n", 0);
		}
	}
	else{
		int returncode = 0;
		waitpid(pid, &returncode, 0);
		write(0, "Printing from the parent process!\n", 0);
		switch(pid){
			case 1: 
				write(0, "child pid is 1 (from parent)\n", 0);
				break;
			case 2:
				write(0, "child pid is 2 (from parent)\n", 0);
				break;
			case 3:
				write(0, "child pid is 3 (from parent)\n", 0);
				break;
			case 4:
				write(0, "child pid is 4 (from parent)\n", 0);
				break;
			case 5:
				write(0, "child pid is 5 (from parent)\n", 0);
				break;
			case 6:
				write(0, "child pid is 6 (from parent)\n", 0);
				break;
			default:
				write(0, "child pid is not between 1 and 6 (from parent)\n", 0);
		}
		pid = getpid();
		switch(pid){
			case 1: 
				write(0, "parent pid is 1 (from parent)\n", 0);
				break;
			case 2:
				write(0, "parent pid is 2 (from parent)\n", 0);
				break;
			case 3:
				write(0, "parent pid is 3 (from parent)\n", 0);
				break;
			case 4:
				write(0, "parent pid is 4 (from parent)\n", 0);
				break;
			case 5:
				write(0, "parent pid is 5 (from parent)\n", 0);
				break;
			case 6:
				write(0, "parent pid is 6 (from parent)\n", 0);
				break;
			default:
				write(0, "parent pid is not between 1 and 6 (from parent)\n", 0);
		}
	}

	return 0;
}
