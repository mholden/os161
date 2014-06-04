#include <stdio.h>
#include <unistd.h>

/*
 * C standard function - print a single character.
 *
 * Properly, stdio is supposed to be buffered, but for present purposes
 * writing that code is not really worthwhile.
 */

int
putchar(int ch)
{
	/* 
	 * This has been edited so that our hack version of the write() 
	 * system call works. We make sure c is null terminated before 
 	 * passing it onto write(). 
	 */
	char c[2];
	c[0] = ch;
	c[1] = '\0';
	int len;
	len = write(STDOUT_FILENO, &c, 1);
	if (len<=0) {
		return EOF;
	}
	return ch;
}
