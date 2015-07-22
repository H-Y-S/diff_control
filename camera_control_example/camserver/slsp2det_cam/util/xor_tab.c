// xor_tab.c -- to set up the xor shift register counter translation table

#include <stdio.h>
#include <math.h>
#include "pix_detector.h"
#include "debug.h"

// the real parameters to be used are defined in detsys.h
//#define NBITS 15
//#define NSTATES 32768	// 2**NBITS

		// feedback is always from the last bit plus one other bit
//#define fbBIT 13		// feed-back bit to use in additon to last bit

/* Results:
** With NBITS=15, get a full sequence with fbBIT=13, 10, 7, 6, 3 & 0.
** With NBITS=18, fbBIT= 10 & 6 are the only ones.
*/

static int xor(int, int);
void xor_add_count(int []);			// needed by test5.c
static int rate_correct(int);



static int state[NBITS];
unsigned int xor_table[NSTATES];
int data_true_level = DATA_TRUE_LEVEL;	// accounts for data inversion

double tau=0.0;							// for rate correction
extern double exposure_time;
static double prev_exposure_time=-1.0;
static double prev_tau=-1.0;
static int prev_zstate = -1;
static int observed_rate_cutoff;
int count_cutoff;


int xor_tab_initialize(int zstate)
{
int i, j, idx, count=0;

// if no change in exposure time, tau or zstate, there is nothing to do

if( (zstate==prev_zstate && (fabs(tau-prev_tau)<1.0e-9) && tau==0.0) ||
		(zstate==prev_zstate && (fabs(tau-prev_tau)<1.0e-9) && 
		(fabs(1.0-prev_exposure_time/exposure_time)<0.0001)) )
	return NSTATES;			// nothing to change

if (zstate!=prev_zstate)
	DBG(4, "XOR Reason: zstate!=prev_zstate, %d!=%d\n", zstate, prev_zstate)
if (fabs(tau-prev_tau)>1.0e-9)
	DBG(4, "XOR Reason: tau change; new tau = %.1lfns, old tau = %.1lfns\n",
			1.0e9*tau, 1.0e9*prev_tau)
if (fabs(1.0-prev_exposure_time/exposure_time)>0.0001)
	DBG(4, "XOR Reason: new exposure time: %.6lf, old exposure time: %.6lf\n",
			exposure_time, prev_exposure_time)

prev_zstate = zstate;
prev_exposure_time = exposure_time;
prev_tau = tau;
if (tau == 0.0)
	{
	observed_rate_cutoff = NSTATES;
	printf("  Building standard xor table\n");
	DBG(1, "  Building standard xor table\n")
	}
else
	{
	observed_rate_cutoff = (MAX_TAU_RATE_PRODUCT/tau)*exp(-MAX_TAU_RATE_PRODUCT);
	if (observed_rate_cutoff * exposure_time > NSTATES-1)
		{
		printf("  Building rate-corrected xor table\n");
		DBG(1, "  Building rate-corrected xor table\n")
		}
	else
		{
		printf("  Building rate-corrected xor table with cutoff = %d counts\n",
					(int)(observed_rate_cutoff * exposure_time));
		DBG(1, "  Building rate-corrected xor table with cutoff = %d counts\n",
					(int)(observed_rate_cutoff * exposure_time))
		}
	}

zstate &= ~(-1<<NBITS);		// mask to NBITS
count_cutoff = 0;

for (i=0; i<NBITS; i++)
	state[i]=1;		// initial state - all 1's
	
for (i=0; i<NSTATES; i++)
	xor_table[i]=zstate;			// empty result table

DBG(3, "Initilizing xor table - data true level = %d, zstate = %d\n",
		data_true_level, zstate)
if (data_true_level == 1)
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
		xor_table[idx] = rate_correct(xor_table[idx]);
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
		xor_table[idx] = rate_correct(xor_table[idx]);
		}
	}

#if 0
printf("Final state: %d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d\n", 
state[19], state[18], state[17], state[16], state[15],
state[14], state[13], state[12], state[11], state[10],
state[9],  state[8],  state[7],  state[6],  state[5],
state[4],  state[3],  state[2],  state[2],  state[0]
	);
printf("--- Final count cutoff: %d\n", count_cutoff);
#endif

if (tau > 0.0)
	{
	printf("  After rate correction, cutoff = %d counts\n", count_cutoff);
	DBG(1, "  After rate correction, cutoff = %d counts\n", count_cutoff)
	}

return NSTATES;		// tell caller how many entries
}

void xor_add_count(int stat[])
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



/*
** rate correction of observed counts
**
** Let o be the observed counts/s, and r the real rate.  Then
**
**    o = r * exp[- r * tau]
**
** using the model of a paralyzable counter with continuous radiation.
** This cannot be inverted in closed form to give r.
**
** We do successive approximations on:
**
**    r = o * exp[r * tau]
**
**    r1 = o * exp[o * tau]
**    r2 = o * exp[r1 * tau], etc.
**  
** But, we need a limit because the entire table of up to 10^6 counts
** must be populated even for exposure times of 1 ms.  So, arbitrarily,
** if the corrected count rate exceeds ~4 x 10^6/s, we return a saturated
** pixel flag.  The actual parameter is set in detsys.h
**
** The non-paralyzable counter model is:
**
**    o = r / (1 + r * tau )
**
** For tau = 194 ns and r = 800,000/s, these models differ by 1%.  Thus,
** this simpler formula may be preferable in some cases.
**
** Also, in some cases, the synchrotron bunch structure should be taken
** into account.
**
** We limit the maximum tau * r product (in detsys.h) so as not to attempt
** to correct rates that are too close to saturation.  This limits the
** maximum corrected rate to:
**
**    rmax = (tau * r max) / tau
**
**  = 2.577e6 if tau = 194e-9.  This maximum corrected rate will be observed
** when 2**20-2 observed counts accumulate in 0.6709 s.
*/

#define CLIM 1.00001		// convergence limit for approximation
static int rate_correct(int observed)
{
double r, r0, obs, z;
int n;

if (tau == 0.0)		// correction turned off
	{
	count_cutoff = max(count_cutoff, observed);
	return observed;
	}

if (exposure_time < 1.0e-5)
	return observed;

if (observed == 0)
	return observed;

obs = observed / exposure_time;		// observed rate

if (obs > observed_rate_cutoff)		// signal counter saturation
	return count_cutoff;

r0 = obs * exp(tau * obs);
for (n=0; n<20; n++)				// generally takes 2 to 10 iterations
	{
	r = obs * exp(tau * r0);
	if (r/r0 < CLIM)
		break;
	r0 = r;
	}

/* - test code - print a few values - see test5.c to set exposure time and tau.
{double q;
if ((observed % 100000) == 0)
	{q = r*exp(-tau*r);
	printf("observed: %.5g, corrected: %.5g, calc: %.5g n = %.2d\n",
			(double)observed, r*exposure_time, q*exposure_time, n);}
}
*/

z = (int)rint(r * exposure_time);
count_cutoff = max(count_cutoff, z);
return z;
}
#undef CLIM
