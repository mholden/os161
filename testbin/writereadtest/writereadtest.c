#include <unistd.h>
#include <string.h>

/*
 * writereadtest - test write() and read()
 */

int
main()
{
	/*
	 * 1. Open a new file on sfs
	 * 2. Write, read, verify, repeat
	 * 3. Close file
	 */

	int fd, len, tot; 
	char write_buf[30], read_buf[30], *err = "Error!\n", *success = "Success!\n";

	fd = open("vol0:file0", O_RDWR | O_CREAT);
	if (fd < 0) write(STDOUT_FILENO, err, strlen(err));

	strcpy(write_buf, "This is our first write!\n");
	tot = 0;
	while((30 - tot) > 0){
		len = write(fd, write_buf + tot, 30 - tot);
		tot += len;
	}
	tot = 0;
	while((30 - tot) > 0){
                len = read(fd, read_buf + tot, 30 - tot);
                tot += len;
        }

	if(!strcmp(read_buf, write_buf)) write(STDOUT_FILENO, success, strlen(success));
	else write(STDOUT_FILENO, err, strlen(err));

	close(fd);
	
	return 0;
}
