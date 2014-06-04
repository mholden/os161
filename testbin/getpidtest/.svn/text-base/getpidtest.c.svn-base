/*
 * getpidtest - test getpid()
 */

#include <unistd.h>
#include <sys/types.h>

int
main(int argc, char *argv[])
{
	(void) argc;
	(void) argv;
	pid_t pid = getpid();
	
	switch(pid){
		case 1: 
			write(0, "pid is 1\n", 0);
			break;
		case 2:
			write(0, "pid is 2\n", 0);
			break;
		case 3:
			write(0, "pid is 3\n", 0);
			break;
		case 4:
			write(0, "pid is 4\n", 0);
			break;
		case 5:
			write(0, "pid is 5\n", 0);
			break;
		default:
			write(0, "pid is not between 1 and 5\n", 0);
	}

	return 0;
}
