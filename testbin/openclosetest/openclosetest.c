#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <err.h>

int
main(int argc, char *argv[])
{
	int fd, err;

	if (argc!=2) errx(1, "Usage: openclosetest <filename>");
	
	fd = open(argv[1], O_RDWR);
	if (fd<0) write(0, "Error on open().\n", 0);
	else write(0, "Looks like open() worked.\n", 0);

	return 0;
}
