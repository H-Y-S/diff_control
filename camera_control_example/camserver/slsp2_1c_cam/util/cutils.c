// cutils.c - miscellaneous utilities for the sls cameras

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "detsys.h"
#include "ppg_util.h"
#include "vxi11core.h"
#include "vxi11.h"
#include "debug.h"


// system globals - used in sls8x2.pkg, included in tvx.c, declared in sls8x2.h
int img_mode=0;


/******************************************************************************\
**                                                                            **
**     XOR decoding of an image                                               **
**                                                                            **
**     If mode=1, fill in double and quad area pixels according               **
**     to the 8x2 module format                                               **
**                                                                            **
\******************************************************************************/

void xor_decode_image(int mode)
{
int i, k, n;
TYPEOF_PIX *modP = theImage;
//char line[120];

for (i=0; i<imageLength; i++)
	theImage[i] = xor_table[theImage[i]];

#ifdef SLSP2_1C_CAMERA
k=n=*modP;		// keep the compiler happy
return;

#else
if (!mode)
	return;

//#ifdef DEBUG
//strcpy(line, cam_image_path);
//strcat(line, "beforecorr.img");
//i = open(line, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
//truncate_image(theImage, imageLength);		// 32 bit to 16 bit
//write(i, theImage, length*sizeof(TYPEOF_PIX));
//close(i);
//#endif

// fill in double and quad area pixels from the pixel that received the counts
// fill in for large sensor pixels by dividing counts
// do not lose the remainder

k = (NROW_MOD/2)*NCOL_BANK;	// the line across the center of the sensor
for (i=0; i<NCOL_MOD; i++)
	{
	n = modP[k + i - NCOL_BANK]/3;		// from pixel above
	modP[k + i - NCOL_BANK] -= n;
	modP[k + i] = n;
	n = modP[k + i + NCOL_BANK]/3;		// from pixel below
	modP[k + i + NCOL_BANK] -= n;
	modP[k + i] += n;
	}
for (i=0; i<NROW_MOD; i++)		// the lines between chips, including the central line
	for (k=0; k<7; k++)
		{
		n = modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) - 1]/3;	// from pixel to left
		modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) - 1] -= n;
		modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) - 0] = n;
		n = modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) + 1]/3;	// from pixel to right
		modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) + 1] -= n;
		modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) + 0] += n;
		}



//#ifdef DEBUG
//strcpy(line, cam_image_path);
//strcat(line, "aftercorr.img");
//i = open(line, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
//truncate_image(theImage, imageLength);		// 32 bit to 16 bit
//write(i, theImage, length*sizeof(unsigned short));
//close(i);
//#endif

#endif

return;
}



/******************************************************************************\
**                                                                            **
**     in place truncatation of an image from 32 bits to 16 bits              **
**                                                                            **
\******************************************************************************/

void truncate_image(unsigned int *img32, int len)
{
int i;
unsigned short *img16 = (unsigned short *)img32;

for(i=0; i<len; i++)
	*img16++ = (unsigned short)(*img32++);

return;
}





#ifdef SLSP2_1M_CAMERA
// system globals 
struct module_pixel *px;

// globals for this module
static struct module_pixel pix_coords;

/*****************************************************************************\
***                                                                         ***
***      pix_add - return module and chip coordinates of a pixel            ***
***                                                                         ***
***     an entry of -1 means that information is sought                     ***
***     an entry >=0 means that information is provided                     ***
***                                                                         ***
\*****************************************************************************/

struct module_pixel *pix_add(int pix, int mx, int my, int cn, int cx, int cy)
{

pix_coords.pix = pix_coords.mx = pix_coords.my = pix_coords.cn =
	pix_coords.cx = pix_coords.cy = -1;

if (pix >= 0)		// given the pixel number
	{
	if (pix>=NCOL_MOD*NROW_MOD)
		return &pix_coords;			// invalid data
	mx = pix % NCOL_MOD;
	my = pix / NCOL_MOD;			// fall through to next if
	}
if (mx >= 0 && my >= 0)	// given the module coordinates of a pixel
	{
	if (mx>=NCOL_MOD || my>=NROW_MOD)
		return &pix_coords;			// invalid data
	pix_coords.pix = mx + my*NCOL_MOD;
	pix_coords.mx = mx;
	pix_coords.my = my;
	if (my < NROW_MOD/2)
		{
		cn = mx/(NCOL_CHIP+1);
		cx = (NCOL_CHIP+1)*(cn+1) - mx - 2;
		if (cx < 0)
			return &pix_coords;		// not in chip
		pix_coords.cn = cn;
		pix_coords.cx = cx;
		pix_coords.cy = my;
		}
	else if (my == NROW_MOD/2)
		return &pix_coords;		// not in chip
	else
		{
		cn = NCHIP/2 + mx/(NCOL_CHIP+1);
		cx = mx % (NCOL_CHIP+2);
		if (cx >= NCOL_CHIP)
			return &pix_coords;		// not in chip
		pix_coords.cn = cn;
		pix_coords.cx = cx;
		pix_coords.cy = NROW_MOD - my - 1;
		}
	}
else			// given the chip number, and the chip coordinates
	{
	if (cn<0 || cx<0 || cy<0)
		return &pix_coords;			// invalid data
	if (cn>=NCHIP || cx>=NCOL_CHIP || cy >= NROW_MOD)
		return &pix_coords;			// invalid data
	pix_coords.cn = cn;
	pix_coords.cx = cx;
	pix_coords.cy = cy;
	if (cn < NCHIP/2)
		{
		pix_coords.mx = (NCOL_CHIP+1)*(cn+1) - cx - 2;
		pix_coords.my = cy;
		}
	else
		{
		pix_coords.mx = (NCOL_CHIP+2)*(cn-NCHIP/2) + cx;
		pix_coords.my = NROW_MOD - cy - 1;
		}
	pix_coords.pix = pix_coords.mx + NCOL_MOD*pix_coords.my;
	}
return &pix_coords;
}
#endif


#ifdef SLSP2DET_CAMERA
// system globals
struct det_pixel *px;

// globals for this module
static struct det_pixel pix_coords;
/*****************************************************************************\
***                                                                         ***
***      pix_add - return module and chip coordinates of a pixel            ***
***                                                                         ***
***     an entry of -1 means that information is sought                     ***
***     an entry >=0 means that information is provided                     ***
***                                                                         ***
\*****************************************************************************/

struct det_pixel *pix_add(int pix, int ix, int iy, int bn, int bx, int by,
		int mn, int mx, int my, int cn, int cx, int cy)
{
pix_coords.pix = pix_coords.ix = pix_coords.iy = pix_coords.bn = pix_coords.bx
		= pix_coords.by = pix_coords.mn = pix_coords.mx = pix_coords.my
		= pix_coords.cn = pix_coords.cx = pix_coords.cy = -1;

if (bn==0)
	return &pix_coords;			// invalid data

if (bn > 0)
	bn--;						// change 1,2,3 system to 0,1,2

if (mn==0)
	return &pix_coords;			// invalid data

if (mn > 0)
	mn--;						// change 1,2,3 system to 0,1,2

if (pix >= 0)					// given the pixel number
	{
	if (pix >= NCOL_DET*NROW_DET)
		return &pix_coords;		// invalid data
	ix = pix % NCOL_DET;
	iy = pix / NCOL_DET;		// fall through to next if
	}
if (ix >= 0 && iy >= 0)			// given image coordinates
	{
	if (ix>=NCOL_DET || iy>=NROW_DET)
		return &pix_coords;				// invalid data
	pix_coords.ix = ix;
	pix_coords.iy = iy;
	by = iy % (NROW_BANK+NROW_BTW_BANK);
	if (by >= NROW_BANK)
		goto finish;				// not in a bank
	bn = iy / (NROW_BANK+NROW_BTW_BANK);
	bx = ix;
	}
if (bn >= 0 && bx >= 0 && by >= 0)	// given bank & coordinates
	{
	if (bn>=NBANKS || bx>=NCOL_BANK || by>=NROW_BANK)
		return &pix_coords;			// invalid data
	pix_coords.bn = bn + 1;			// banks numbered 1,2...
	pix_coords.bx = bx;
	pix_coords.by = by;
	mx = bx % (NCOL_MOD+NCOL_BTW_MOD);
	if (mx >= NCOL_MOD)
		goto finish_det;				// not in a module
	mn = bx / (NCOL_MOD+NCOL_BTW_MOD);
	my = by;
	}
if (bn >= 0 && mn >= 0 && mx >= 0 && my >= 0)	// given bank, module & coordinates
	{
	if (bn>=NBANKS || mn>=NMOD_BANK || mx>=NCOL_MOD || my>=NROW_MOD)
		return &pix_coords;			// invalid data
	pix_coords.mn = mn+1;				// modules numbered 1,2...
	pix_coords.mx = mx;
	pix_coords.my = my;
	if (my < NROW_MOD/2)
		{
		cn = mx/(NCOL_CHIP+1);
		cx = (NCOL_CHIP+1)*(cn+1) - mx - 2;
		if (cx < 0)
			goto finish_bank;		// not in a chip
		pix_coords.cn = cn;
		pix_coords.cx = cx;
		pix_coords.cy = my;
		}
	else if (my == NROW_MOD/2)
		goto finish_bank;		// not in a chip
	else
		{
		cn = NCHIP/2 + mx/(NCOL_CHIP+1);
		cx = mx % (NCOL_CHIP+2);
		if (cx >= NCOL_CHIP)
			goto finish_bank;		// not in a chip
		pix_coords.cn = cn;
		pix_coords.cx = cx;
		pix_coords.cy = NROW_MOD - my - 1;
		}
	goto finish_bank;
	}
else if (bn >= 0 && mn >= 0 && cn >= 0 && cx >=0 && cy >= 0)
	{
	if (bn>=NBANKS || mn>=NMOD_BANK || cn>=NCHIP || cx>=NCOL_CHIP || cy>=NROW_CHIP)
		return &pix_coords;			// invalid data
	pix_coords.cn = cn;
	pix_coords.cx = cx;
	pix_coords.cy = cy;
	pix_coords.mn = mn+1;		// modules numbered 1,2...
	if (cn < NCHIP/2)
		{
		pix_coords.mx = (NCOL_CHIP+1)*(cn+1) - cx - 2;
		pix_coords.my = cy;
		}
	else
		{
		pix_coords.mx = (NCOL_CHIP+2)*(cn-NCHIP/2) + cx;
		pix_coords.my = NROW_MOD - cy - 1;
		}
	goto finish_bank;
	}
else
	return &pix_coords;			// invalid data

finish_bank:
pix_coords.bx = pix_coords.mx + mn*(NCOL_MOD+NCOL_BTW_MOD) ;
pix_coords.by = pix_coords.my;

finish_det:
pix_coords.bn = bn+1;		// banks numbered 1,2...
pix_coords.ix = pix_coords.bx;
pix_coords.iy = pix_coords.by + bn*(NROW_BANK+NROW_BTW_BANK);;

finish:
pix_coords.pix = pix_coords.ix + NCOL_DET*pix_coords.iy;

return &pix_coords;
}
#endif


/*****************************************************************************\
***                                                                         ***
***      log_gpib_readings -- append readings to 'treport.txt'              ***
***                                                                         ***
\*****************************************************************************/

void log_gpib_readings(FILE *ofp)
{
#ifdef USE_GPIB
double voltages[7];
char line[300];

if (rpcopen("hpib,9"))
	{
	printf("*** Could not open RPC connection for GPIB readout\n");
	fprintf(ofp, "*** Could not open RPC connection for GPIB readout\n");
	fprintf(ofp, "\n-----------------------------------------------------------\n");
	return;
	}
/*gpibsend("MEAS:VOLT:DC? (@101,102,103,104,105,106,107)");*/
gpibsend("READ?");
gpibread(line);
rpcclose();
fprintf(ofp, "----------- Voltages & currents reported from GPIB ----------\n\n");
sscanf(line, "%lf,%lf,%lf,%lf,%lf,%lf,%lf", voltages+0, voltages+1,
		voltages+2,voltages+3,voltages+4,voltages+5,voltages+6);
fprintf(ofp, "Ignd   %.4g mA\n", 1000.0*voltages[0]);
fprintf(ofp, "Ia-    %.4g mA\n", 1000.0*voltages[1]);
fprintf(ofp, "Ic-    %.4g mA\n", 1000.0*voltages[2]);
fprintf(ofp, "Id-    %.4g mA\n", 1000.0*voltages[3]);
fprintf(ofp, "Vgnd   %.4g V\n", voltages[5]);
fprintf(ofp, "Vtrm   %.4g V\n", voltages[4]);
fprintf(ofp, "Vrf    %.4g V\n", voltages[6]);
fprintf(ofp, "\n-----------------------------------------------------------\n");
#endif

return;
}

/*
gpib_offset
{
if (rpcopen())
	{
	printf("*** Could not open RPC connection for GPIB readout\n");
	return;
	}
gpibsend("MEAS:VOLT:DC? (@101,102,103,104,105,106,107)")
//gpibsend("SENS:VOLT:DC:RANG 1, (@101,102,103,104)");
//gpibsend("SENS:VOLT:DC:RANG 10,(@105)");
//gpibsend("SENS:VOLT:DC:RES 0.001,(@101,102,103,104,105)");
//gpibsend("CALC:SCALE:OFFSET:NULL (@101,102,103,104)");
//gpibsend("CALC:SCALE:STATE ON, (@101,102,103,104)");
rpcclose();
return;
}
*/

#ifdef USE_GPIB

void gpibsend_vo(char *line)
{
if (rpcopen("hpib,9"))
	{
	printf("*** Could not open RPC connection for GPIB dvm\n");
	return;
	}
gpibsend(line);
rpcclose();
return;
}

void gpibsend_os(char *line)
{
if (rpcopen("hpib,5"))
	{
	printf("*** Could not open RPC connection for GPIB oscilloscope\n");
	return;
	}
gpibsend(line);
rpcclose();
return;
}
#define MAX_N_VOLTAGES 22

void gpibread_vo(void)
{
int i, count;
char line[20*MAX_N_VOLTAGES], *p;
double voltages[MAX_N_VOLTAGES];
if (rpcopen("hpib,9"))
	{
	printf("*** Could not open RPC connection for GPIB dvm\n");
	return;
	}
gpibsend("READ?");
gpibread(line);
rpcclose();
for (i=0, p=line, count=0; i<MAX_N_VOLTAGES; i++)
	{
	if (sscanf(p, "%lf", voltages+i) )
		count++;
	if ( (p=strchr(p, ',')) == NULL)
		break;		// we're done
	p++;
	}
for (i=0; i<count; i++)
	printf("channel %d = %.4g\n", 101+i, voltages[i]);
return;
}

void gpibread_os(char *cmd)
{
char line[200], *q, *qq;
double ampl, scale=1000.0;
int x, y;

if (rpcopen("hpib,5"))
	{
	printf("*** Could not open RPC connection for GPIB oscilloscope\n");
	return;
	}
/*gpibsend("TA:PAVA? AMPL");*/
if ( (q=strchr(cmd, '!')) )		// optional extension = (x,y) address, scale
	{
	*q='\0';					// truncate cmd for gpibsend()
	q+=3;						// skip over possible comma
	while ( (qq=strchr(q, ',')) )
		*qq=' ';				// change optional commas to spaces
	sscanf(q, "%d%d%lf", &x, &y, &scale);	// get address & scale
//	printf("%s, x= %d, y= %d, scale = %lf\n",q,x,y,scale);
	}
gpibsend(cmd);
gpibread(line);
rpcclose();
printf("response: %s\n",line);
qq=line;
if (q)
	{
	while ( (qq=strchr(qq, ',')) )
		{
		qq++;				// look for comma - number
		if (isdigit(*qq) || *qq=='-')
			break;
		}
	ampl=0.0;
	if (qq)
		sscanf(qq, "%lf", &ampl);
	else
		printf("gpibread_os - data not found\n");
//	theImage[x + y*IMAGE_NCOL]=(int)rint(fabs(1000.0*ampl));
	theImage[x + y*IMAGE_NCOL]=(int)rint(fabs(scale*ampl));
	}
/*printf("Amplitude = %.1g\n", ampl*1000);*/
return;
}

#endif

/*****************************************************************************\
***                                                                         ***
***         moduleFilename - same as in tvx                                 ***
***                                                                         ***
\*****************************************************************************/

// Make up module filenames according to enabled options. E.g.:
// sls42:      /path/base_name.dat
// sls8x2:     /path/base_name.dat    
// slsdet:     /path/base_name_b3_m1.dat
void moduleFilename(char *line, char *path, char *base, int bank, int module)
{
char tmp[50], *p, *q;

if (base[0]=='/' || base[0]=='.')	// skip leading './' etc.
	base++;
if (base[0]=='/' || base[0]=='.')
	base++;
strcpy(tmp, base);
if ( (q = strchr(tmp, '.')) )		// remove extension at first '.'
	*q = '\0';
if ( (p = strstr(tmp, "_c")) && isdigit(*(p+2)) )	// remove old suffixes
	*p = '\0';
if ( (p = strstr(tmp, "_m")) && isdigit(*(p+2)) )
	*p = '\0';
if ( (p = strstr(tmp, "_b")) && isdigit(*(p+2)) )
	*p = '\0';

if (path)				// 'line' may already contain path
	strcpy(line, path);
strcat(line, tmp);		// base file name

#ifdef SLSDET_CAMERA
sprintf(line+strlen(line), "_b%.2d_m%.2d", bank, module);
#endif

if (q)					// if an extension was given
	{
	*q = '.';
	strcat(line, q);
	}

return;
}

