// settrims.c - program trims from an array of values

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include "i2c_util.h"
#include "dcblib.h"
#include "debug.h"




/******************************************************************************\
**                                                                            **
**          Global variables                                                  **
**                                                                            **
\******************************************************************************/

#ifdef SLSP2DET_CAMERA
short trimValues[NBANK][NMOD_BANK][NCHIP][NROW_CHIP][NCOL_CHIP];
#else
 #error
#endif


/******************************************************************************\
**                                                                            **
**     Programming trim bits                                                  **
**                                                                            **
\******************************************************************************/


#ifdef SLSP2DET_CAMERA


// set individual trims from values in array
void setTrims(void)
{
int y, x, devno, t;

#ifdef MASTER_COMPUTER
if (selected_bank > NDCB)	// master only keeps records
  	return;
#endif

printf("setTrims for p2 detector\n");

if (selected_chip<0 || selected_chip>=NCHIP)
	{
	printf ("Selected chip is: %x; exactly 1 chip must be selected\n",
			selected_chip);
	return;
	}

if (selected_mod<=0 || selected_mod>NMOD_BANK)
	{
	printf ("Selected mod is: %x; exactly 1 module must be selected\n",
			selected_mod);
	return;
	}

if (selected_bank<=0 || selected_bank>NBANK)
	{
	printf ("Selected bank is: %x; exactly 1 bank must be selected\n",
			selected_bank);
	return;
	}

// check for legal values
for (y = 0; y < NROW_CHIP; y++)
	for (x = 0; x < NCOL_CHIP; x++)
		{
		t = trimValues[selected_bank-1][selected_mod-1][selected_chip][y][x];
		if (t <0 || t>MAX_TRIM_VALUE)
			{
			printf ("Trim array is not properly loaded\n");
			return;
			}
		}

// 1) reset the fifo
devno = dcb_get_gsdevno(selected_bank, selected_mod);
dcb_trim_fifo_reset(devno);
dcb_insert_row_col_token(devno);

// 2) fill the fifo
for (y = 0; y < NROW_CHIP; y++)
	for (x = 0; x < NCOL_CHIP; x++)
		dcb_trim_fifo_ld(devno,
				trimValues[selected_bank-1][selected_mod-1][selected_chip][y][x]);

// 3) give the trim p command
dcb_trim_pixels(devno);

DBG(3, "Finished setting trims from array\n")
return;
}
#else
 #error
#endif		// SLSP2DET_CAMERA



#ifdef SLSP2DET_CAMERA
void init_trim_array(int value)
{
int i, j, k, x, y;

for (k=0; k<NBANK; k++)
	for (j=0; j<NMOD_BANK; j++)
		for (i=0; i<NCHIP; i++)
			for (y=0; y<NROW_CHIP; y++)
				for (x=0; x<NCOL_CHIP; x++)
					trimValues[k][j][i][y][x] = value;

return;
}
#endif


