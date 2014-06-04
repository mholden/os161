#include <types.h>
#include <lib.h>
#include <syscall.h>

/* 
 * The read() syscall.
 */

int sys_read(int filehandle, void *buf, size_t max_size){
	// For now just keep it simple.
	(void) filehandle;
	
	// This is pretty hack.. not sure if it will work for all programs

	size_t pos = 0;
	int ch;
	char *_buf = (char *) buf;

	while (1) {
		if(pos == max_size) break;
		ch = getch();
		if (ch=='\n' || ch=='\r'){
			ch = '\n';
			putch(ch);
		}
		_buf[pos++] = ch;
	}

	return 0; // Assume success.
}
