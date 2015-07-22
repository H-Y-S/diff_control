// xor_tab.c -- to set up the xor shift register counter translation table

#include <stdio.h>
#include "pix_detector.h"

//#define NBITS 15
//#define NSTATES 32768	// 2**NBITS

		// feedback is always from the last bit plus one other bit
//#define fbBIT 13		// feed-back bit to use in additon to last bit

/* Results:
** With NBITS=15, get a full sequence with fbBIT=13, 10, 7, 6, 3 & 0.
** With NBITS=18, fbBIT= 10 & 6 are the only ones.
*/

static int xor(int, int);
static void xor_add_count(int []);

static int state[NBITS];
unsigned int xor_table[NSTATES];
int fifo_data_true_level = FIFO_DATA_TRUE_LEVEL;	// accounts for data inversion


int xor_tab_initialize(int zstate)
{
int i, j, idx, count=0;

zstate &= ~(-1<<NBITS);		// mask to NBITS

for (i=0; i<NBITS; i++)
	state[i]=1;		// initial state - all 1's
	
for (i=0; i<NSTATES; i++)
	xor_table[i]=zstate;			// empty result table

if (fifo_data_true_level == 1)
	{
	for (j=0; j<NSTATES-1; j++)			// simulate NSTATES-1 counts
		{
		xor_add_count(state);
		count++;
		idx = 0;
		for (i=0; i<NBITS; i++)		// put the resulting bit pattern in table
			if  ( (state[i]) )
				idx |= 1<<i;
		xor_table[idx] = (count % (NSTATES-1));	// state 0 is set at end
		}
	}
else			// inverted data
	{
	for (j=0; j<NSTATES-1; j++)			// simulate NSTATES-1 counts
		{
		xor_add_count(state);
		count++;
		idx = 0;
		for (i=0; i<NBITS; i++)		// put the resulting bit pattern in table
			if  ( !(state[i]) )
				idx |= 1<<i;
		xor_table[idx] = (count % (NSTATES-1));	// state 0 is set at end
		}
	}

return NSTATES;		// tell caller how many entries
}

static void xor_add_count(int stat[])
{
int i;
int new_state[NBITS];

// the general case -- shift-register without feedback-- note that 0 is not set
for (i=0; i<NBITS-1; i++)
	new_state[i+1] = stat[i];

// now, the specific feedback -- always the last bit plus one other
new_state[0]= xor(stat[NBITS-1], stat[fbBIT]);

// copy the new state into state register
for (i=0; i<NBITS; i++)
	stat[i] = new_state[i];

return;
}

static int xor(int a, int b)
{
if (a == 0)
	return b;
else
	return !b;
}
