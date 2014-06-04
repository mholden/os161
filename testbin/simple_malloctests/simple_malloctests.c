/*
 * simple_malloctests - test malloc()
 */

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>

int
main(int argc, char *argv[])
{
	(void) argc;
	(void) argv;

	char *string1_const = "WE ARE TESTING MALLOC\n";
	char *string1;

	string1 = malloc(strlen(string1_const) + 1); // 23 BYTES
	strcpy(string1, string1_const);

	printf(string1);

	return 0;
}
