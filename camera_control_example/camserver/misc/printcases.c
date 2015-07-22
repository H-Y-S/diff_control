// printcases.c - print out the numeric values of cases in camserver.h

// gcc -g  -Wall -o printcases printcases.c

#include <stdio.h>
#include <stdlib.h>
#define CAMERA_MAIN
#define DEBUG
#include "../include/camserver.h"

int main(int agrc, char *argv[])
{
int i;

for(i=0; i<camera_count; i++)
	printf("%16s = %3d,\n", camera_list[i].name, (int)camera_list[i].cmnd);

return 0;
}
