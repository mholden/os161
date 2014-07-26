#include <unistd.h>
#include <stdio.h>
#include <string.h>

/*
 * Testing all file system system calls:
 * 	open, read, write, lseek, close,
 *	dup2, chdir, getcwd, mkdir, rmdir,
 * 	getdirentry, fstat, remove, rename,
 * 	sync, fsync
 */

#define BUF_SIZE 30

int
main()
{
	int err, fd0, fd1, fd2, fd3, fd4, len, tot; 
	char write_buf[BUF_SIZE] = { NULL }, read_buf[BUF_SIZE] = { NULL };
	void *pointer_err;

	/* Run this from cwd = emu0: */
	pointer_err = getcwd(read_buf, sizeof(read_buf));
	if(pointer_err == NULL){
		printf("getcwd error!\n");
		return -1;
	} //else printf("getcwd success! cwd = %s\n", read_buf);
	
	err = chdir("vol0:");
	if(err){
                printf("chdir error!\n");
                return -1;
        } //else printf("chdir success!\n");

	pointer_err = getcwd(read_buf, sizeof(read_buf));
        if(pointer_err == NULL){
                printf("getcwd error!\n");
                return -1;
        } //else printf("getcwd success! cwd = %s\n", read_buf);

	fd0 = open("vol0:file0", O_RDWR | O_CREAT);
	if (fd0 < 0){
		printf("open fd0 error!\n");
		return -1;
	} //else printf("open fd0 success!\n");

	fd1 = open("vol0:file1", O_RDWR | O_CREAT);
        if (fd1 < 0){
                printf("open fd1 error!\n");
                return -1;
        } //else printf("open fd1 success!\n");

	fd2 = dup2(fd1, 9);
	if (fd2 < 0){
                printf("dup2 error!\n");
                return -1;
        } //else printf("dup2 success!\n");

	strcpy(write_buf, "Write to file0!\n");
	tot = 0;
	while((BUF_SIZE - tot) > 0){
		len = write(fd0, write_buf + tot, BUF_SIZE - tot);
		tot += len;
	}
	
	if(lseek(fd0, 0, SEEK_SET) < 0){
		printf("lseek error!\n");
                return -1;
	}

	tot = 0;
	while((BUF_SIZE - tot) > 0){
                len = read(fd0, read_buf + tot, BUF_SIZE - tot);
                if(len == 0) break; // EOF	
		tot += len;
        }

	if(strcmp(read_buf, write_buf)){
		printf("write, lseek, or read to/from fd0 unsuccessful!\n");
		return -1;
	}
	else //printf("write, lseek, and read to/from fd0 successful and consistent!\n");

	if(close(fd1)){
		printf("close fd1 error!\n");
                return -1;
	}

	strcpy(write_buf, "Write 1 to file1!\n");
        tot = 0;
        while((BUF_SIZE - tot) > 0){
                len = write(fd2, write_buf + tot, BUF_SIZE - tot);
                tot += len;
        }

	strcpy(write_buf, "Write 2 to file1!\n");
        tot = 0;
        while((BUF_SIZE - tot) > 0){
                len = write(fd2, write_buf + tot, BUF_SIZE - tot);
                tot += len;
        }

	/*
	printf("Writes to file1 appear successful. p /bin/cat vol0:file1 should output:\n"
	       "Write 1 to file 1!\nWrite 2 to file 1!\n");
	*/

	err = remove("file0");
	if(err){
		printf("remove error!\n");
                return -1;
	}

	err = rename("file1", "file1_renamed");
	if(err){
                printf("rename error!\n");
                return -1;
        }

	/* This should work.. still have fd0 open even though we've "removed" it */
	err = fsync(fd0);
	if(err){
                printf("fsync error!\n");
                return -1;
        }

	err = sync();
        if(err){
                printf("sync error!\n");
                return -1;
        }

	fd3 = open("vol0:file3", O_RDWR | O_CREAT);
        if (fd3 < 0){
                printf("open fd0 error!\n");
                return -1;
        }

	fd4 = open("vol0:file4", O_RDWR | O_CREAT);
        if (fd4 < 0){
                printf("open fd0 error!\n");
                return -1;
        }

	/* Do an ls on vol0: */
	/* Test mkdir, rmdir, getdirentry, and fstat */

	/* Don't close fd0 or fd2 and make sure things get cleaned up corretly */

	return 0;
}
