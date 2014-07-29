/*
 * f_write.c
 *
 *	This used to be a separate binary, because it came from Nachos
 *	and nachos didn't support fork(). However, in OS/161 there's
 *	no reason to make it a separate binary; doing so just makes
 *	the test flaky.
 *
 *
 * 	It will start writing into a file, concurrently with
 * 	one or more instances of f_read.
 *
 */

#define SectorSize   512

#define TMULT        50
#define FSIZE        ((SectorSize + 1) * TMULT)

#define FNAME        "f-testfile"
#define READCHAR     'r'
#define WRITECHAR    'w'

#include <unistd.h>
#include <stdio.h>
#include <err.h>
#include "f_hdr.h"

static char buffer[SectorSize + 1];

void
subproc_write(void)
{
	int fd, tot, len;
	int i;

	for (i=0; i < SectorSize + 1; i++) {
		buffer[i] = WRITECHAR;
	}
  
	printf("File Writer starting ...\n");

	fd = open(FNAME, O_WRONLY);
	if (fd < 0) {
		err(1, "%s: open", FNAME);
	}

	for (i=0; i<TMULT; i++) {
		// yield();
		tot = 0;
                while(((SectorSize + 1) - tot) > 0){
                        len = write(fd, buffer + tot, (SectorSize + 1) - tot);
                        if(len < 0) err(1, "[BIGFILE]: write");
                        if(len < SectorSize + 1){
				/*
				 * If we write < SECTOR_SIZE + 1, context switch to the read,
				 * then the read thread reads that whole sector and does a 
				 * "check_buffer()" (which checks to ensure all characters
				 * read are the same), that check_buffer() will fail. These
				 * tests are flawed.. read() and write() do not always read
				 * and write the full number of bytes passed in, which is why
				 * we need to put them in these loops.
				 */
				printf("Test is flawed - ignore results!\n");
			}
			tot += len;
                }
	}

	close(fd);

	printf("File Write exited successfully!\n");
}
