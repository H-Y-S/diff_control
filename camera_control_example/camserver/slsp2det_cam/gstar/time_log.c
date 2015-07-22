// time_log.c - a high precision, low overhead time-point logger

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>

#include "gslib.h"
#include "dcblib.h"
#include "time_log.h"

/* ======================================================================== */
/* ======================================================================== */
/* ======================================================================== */

/*
** time_log()  --  Gather statistics on time points
**
** requires gslib.c and the PMC GigaSTaR driver.
** However, since we doesn't use the PMC GigaSTaR card here, this driver
** function could be separated out.
**
** Conceptually, time_log is a spread-sheet that is filled in line by line.
** When finished, it prints out stats on the time points.
**
** Usage, e.g.:
**
	time_log(-1, NULL);					// start the time-point logger
	for (i=0; i<-1+n_images; i++)
		{
		...
		t0 = time_log(1, "full cycle");		// start a new line
		...
		dt = time_log(0, "exposure") - t0;	// time point with label
		...
		time_log(0, "readout");				// time point with label
		...
		}
	...
	time_log(2, NULL);					// print the timing results
**
** Each line in the output shows the statitistics of the time interval
** formed by subtracting the time of the previous line from the time at
** the current line.  The label you supply is also printed.
**
** The line labeled "full cycle" is printed last, and represents the
** average complete cycle of the timed function.
**
** Calls to this function cost ~1 us (3 GHz Xeon)
**
** specific example for the results below
**
	time_log(-1, NULL);
	for(i=0;i<100;i++)
		{
		time_log(1, "full cycle");
		usleep(10000);
		time_log(0, "after sleep");
		}
	time_log(2, NULL);
**
**
** Some results (3 GHz Xeon):
**
   sleep(1);		// gives 1001 ms
   usleep(10000);	// gives 12.0 ms
   usleep(1000);	// gives 3.0 ms
   usleep(100);		// gives 2.0 ms
   usleep(10);		// gives 2.0 ms
   usleep(1);		// gives 2.0 ms

**
** If the logger is not set up, or if there are too many time points
** requested, the function returns the gstime() at the time of the call.
** Thus, this function can be used a s substitute for gstime(), no 
** matter whether it is activated.
*/
#define TDSIZE 20		// maximum number of time points
static struct {	double tn;		// time now
				double sdt;		// sum delta t
				double sdt2;	// sum^2 delta t
				double min;		// min delta t
				double max;		// max delta t
				int ct;			// count
				char *cp;		// comment pointer
			  } td[TDSIZE];		// time data
static int tdidx, tlsetup=0;
static double sys_t0, gs_t0;

double time_log(int fn, char *comment)
{
int i;
double dt, z=gs_time();
char line[80];
struct timeval tv;
struct timezone tz;

switch (fn)
	{
	case -1:			// initialize
		tdidx=0;
		memset(td, 0, sizeof(td));
		for(i=0; i<TDSIZE; i++)
			td[i].min = 1.0e50;
		gs_t0 = z;
		gettimeofday(&tv, &tz);
		sys_t0 = tv.tv_sec + ((double)tv.tv_usec)/1.0e6;
		tlsetup = 1;
		break;

	case 0:				// record a time point
		if (!tlsetup)
			break;
		if(tdidx >= TDSIZE)
			break;
		dt=z-td[tdidx-1].tn;
		td[tdidx].sdt += dt;
		td[tdidx].sdt2 += dt*dt;
		td[tdidx].min = min(dt, td[tdidx].min);
		td[tdidx].max = max(dt, td[tdidx].max);
		td[tdidx].ct++;
		td[tdidx].tn = z;
		td[tdidx].cp = comment;
		tdidx++;
		break;

	case 1:				// start a "new line" and record a point
		if (!tlsetup)
			break;
		tdidx=0;
		if (td[tdidx].tn > 0.0)
			{
			dt=z-td[tdidx].tn;
			td[tdidx].sdt += dt;
			td[tdidx].sdt2 += dt*dt;
			td[tdidx].min = min(dt, td[tdidx].min);
			td[tdidx].max = max(dt, td[tdidx].max);
			td[tdidx].ct++;
			td[tdidx].cp = comment;
			}
		td[tdidx].tn = z;
		tdidx++;
		break;

	case 2:				// print results
		if (!tlsetup)
			break;
		gettimeofday(&tv, &tz);
		dt = tv.tv_sec + ((double)tv.tv_usec)/1.0e6;
		printf("System delta t  = %.1lf ms\n", 1.0e3*(dt - sys_t0));
		printf("CPU tsc delta t = %.1lf ms\n", 1.0e3*(z - gs_t0));
		if (td[0].ct < 2)
			{
			printf("Not enough data points in time_log\n");
			return 0.0;
			}
		for(i=1; i<TDSIZE; i++)
			{
			if (!td[i].ct)		// if 0, we're done
				break;
			td[i].sdt /= td[i].ct;
			td[i].sdt2 /= td[i].ct;
			z = sqrt(td[i].sdt2 - td[i].sdt*td[i].sdt);
			sprintf(line, "%s", td[i].cp);
			sprintf(line+strlen(line), "                               ");
			sprintf(line+20, ": avg = %7.2lf, sd = %6.2lf, min = %7.2lf, max = %7.2lf",
					1.0e3*td[i].sdt, 1.0e3*z, 1.0e3*td[i].min, 1.0e3*td[i].max);
			printf("%s\n", line);
			}
		i = 0;					// summary of entire cycle
		td[i].sdt /= td[i].ct;
		td[i].sdt2 /= td[i].ct;
		z = sqrt(td[i].sdt2 - td[i].sdt*td[i].sdt);
		sprintf(line, "summary - %s", td[i].cp);
		sprintf(line+strlen(line), "                                     ");
		sprintf(line+20, ": avg = %7.2lf, sd = %6.2lf, min = %7.2lf, max = %7.2lf",
				1.0e3*td[i].sdt, 1.0e3*z, 1.0e3*td[i].min, 1.0e3*td[i].max);
		printf("%s\n", line);
		tlsetup = 0;
		break;

	default:
		tlsetup = 0;
		break;
	}
return z;
}

