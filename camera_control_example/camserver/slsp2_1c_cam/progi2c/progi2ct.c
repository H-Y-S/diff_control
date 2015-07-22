// dac1t.c - program to test dac1, the pattern composer

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "ppg_util.h"
#include "pix_detector.h"
#include "debug.h"

#define DEF_FILE "../cam_data/p2_1mod.def"

#ifdef DEBUG
FILE *dbgofp;   // global output file
int dbglvl;     // debugging control level
#endif

char cam_data_path[80] = "./";
char cam_image_path[80] = "./";

// Main program to test pattern composer

int main (int argc, char *argv[])
{
int ret, i, key=0;
unsigned short *p, selector;
unsigned short ush[10];
char line[20];
FILE *ofp;

#ifdef DEBUG
debug_init("debug.out", NULL);   // turn on debugging
dbglvl = 9;     // and turn it all the way up
#endif

// the following check on linking
if(key)
	set_canonical_form(line);

if(key)
	vpg_execute_pattern(ush, i, 2);

if(key)
	fifo_read_status();

DBG(5, "Debugging started\n");

read_detector_info(DEF_FILE);	// fill the structures from the detector definition

// Set the initial pattern to all 0
for (i = 0; i < PAT_LNG; i++)
	thePattern[i] = 0;

// baseWord is the running word to be output to pattern set
// we set a couple of unused bits to see that they are preserved
baseWord = 0xf000;
for (i = 0; i < NpSig; i++)		// install quiescent state
	{
	if ( (pGen[i].quiescent == 1 && pGen[i].logic == 1)  || 
					(pGen[i].quiescent == 0 && pGen[i].logic == 0) )
			baseWord |= 1 << pGen[i].bit_no;	// set negative logic lines to 1 = off
	}
patternP = thePattern;	// point to start of pattern
*patternP++ = baseWord;		// put the starting state ("quiescent") in the pattern
ret = set_dac("B5_M1_VCMP3", 0.85);
if (ret)
	printf("Error return from pattern composer\n");
else
	printf("Successful return from the pattern composer\n");

if ((ofp=fopen("/tmp/patdac.dat", "w")) == NULL)
	{printf("Unable to open pattern output file for writing\n");
	exit (1);
	}
for (p=thePattern; p<patternP; p++)
	{
	selector = 0x8000;
	for (i=0; i<16; i++)
		{
		if (selector & *p)
			fprintf(ofp, "1");
		else
			fprintf(ofp, "0");
		selector = selector >> 1;
		}
	fprintf(ofp, "\n");
	}

fclose(ofp);

// Set the initial pattern to all 0
for (i = 0; i < PAT_LNG; i++)
	thePattern[i] = 0;

// baseWord is the running word to be output to pattern set
// we set a couple of unused bits to see that they are preserved
baseWord = 0x180;
for (i = 0; i < NpSig; i++)		// install quiescent state
	{
	if ( (pGen[i].quiescent == 1 && pGen[i].logic == 1)  || 
					(pGen[i].quiescent == 0 && pGen[i].logic == 0) )
			baseWord |= 1 << pGen[i].bit_no;	// set negative logic lines to 1 = off
	}
patternP = thePattern;	// point to start of pattern
*patternP++ = baseWord;		// put the starting state ("quiescent") in the pattern
ret = set_chsel("B9_M3_CHSEL15", 1);
if (ret)
	printf("Error return from pattern composer\n");
else
	printf("Successful return from the pattern composer\n");

if ((ofp=fopen("/tmp/patchsel.dat", "w")) == NULL)
	{printf("Unable to open pattern output file for writing\n");
	exit (1);
	}
for (p=thePattern; p<patternP; p++)
	{
	selector = 0x8000;
	for (i=0; i<16; i++)
		{
		if (selector & *p)
			fprintf(ofp, "1");
		else
			fprintf(ofp, "0");
		selector = selector >> 1;
		}
	fprintf(ofp, "\n");
	}

fclose(ofp);
printf("/tmp/patchsel.dat and /tmp/patdac.dat were written\n");

#ifdef DEBUG
fclose(dbgofp);
#endif

return 0;
}


