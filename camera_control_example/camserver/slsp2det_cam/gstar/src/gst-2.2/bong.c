// bong.c - reset the gigastar interrupt

// gcc -g -Wall util/gslib.o -o bong bong.c

#include <stdio.h>
#include "include/gslib.h"

int main (int argc, char *argv[])
{

if (gs_initialize(NULL))
	printf("Bad return from gs_initialize()\n");

gs_clear_irq(0);

return 0;
}
