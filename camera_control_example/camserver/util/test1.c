/* test.c -- its sole function is to test linking in ../util */

#include <stdio.h>
#include "debug.h"
#include "tvxobject.h"
#include "imageio.h"
#include "../../../include/tvxsys.h"

int DefaultHigh=1024;
int DefaultWide=1024;
int DefaultBpp=16;

#ifdef DEBUG
FILE *dbgofp;   // global output file
int dbglvl;     // debugging control level
#endif

int main (int argc, char *argv[])

{
char line[20];
int key=0;
struct object_descriptor *OP=NULL;

printf("Starting test\n");

#ifdef DEBUG
// Start debugger
        debug_init("camdbg.out", NULL);
        dbglvl=9;               // full details
#endif

printf("time: %s\n", timestamp());

if (key)
	fill_basic_header(OP, NULL);

if (key)
	write_diskimage(OP, 0, 0, line);

printf("Test completed\n");
return 0;
}


/* dummy routines to satisfy linking */
int write_line(const char *fmt, ...) 
{return 0;}

int display_width(void)
{return 0;}

int set_image_sign(struct object_descriptor *a)
{return 0;}

int reset_image_sign(struct object_descriptor *a)
{return 0;}

int CheckObject(struct object_descriptor *a)
{return 0;}

int QueueForRewrite(struct object_descriptor *a)
{return 0;}

