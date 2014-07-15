#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <err.h>

int
main()
{
	int fd0, fd1, fd2;
	char *error = "Error.\n", *done = "Done openclosetest.\n";
	
	fd0 = open("lhd1raw:", O_RDWR);
	if (fd0 < 0) write(1, error, strlen(error));

	/* Assumes we have done a mksfs lhd1raw: vol0 and a mount */	
	fd1 = open("vol0:file0", O_RDWR | O_CREAT);
	if (fd1 < 0) write(1, error, strlen(error));
	
	fd2 = open("vol0:file1", O_RDWR | O_CREAT);
	if (fd2 < 0) write(1, error, strlen(error));

	close(fd1);

	write(1, done, strlen(done));

	/* 
	 * Leave fd0 and fd2 unclosed.. make sure thread_exit 
	 * cleans things up properly.
	 */

	return 0;
}
