/* commando.h - definitions of enums and structures for commando.c
**
** This file is dynamically updated during compilation.  User header info
** may be safely placed up to the line "start of COMMAND enum..."  Lines
** below that will be altered.
*/

#ifndef COMMANDO_H
#define COMMANDO_H

// user header info can be put here - it will be preserved

#define TextBufferLength 500
#define LINELNG TextBufferLength
#define MaxRows 1


#undef COMMAND		/* will be redefined below */

/* start of COMMAND enum - please do not change this comment - lines below will change */
typedef enum {
		T0 = 1,
		Rdt,
		Timestamp,
		TimeT,
		status,
		reset,
		Start,
		Bbar0r,
		FullStat,
		FIrqstat,
		Ioctl,
#define NCYCLES 4096
#define NEXTRAS 10
		Pio1,
#undef NCYCLES
#undef NEXTRAS
#define NPAGES 57
		Test18,
#undef NPAGES
#define NTRIES 10000
#define NPAGES 57
		Test19,
#undef NPAGES
#undef NTRIES
#define NTRIES 1000
#define NPAGES 57
		Test20,
#undef NPAGES
#undef NTRIES
#define NPAGES 57
		Test21,
#undef NPAGES
#define NTRIES 3000	
#define NPAGES 57
		Test22,
#undef NPAGES
#undef NTRIES
#ifdef _SLS_CAMERA_
#define NIMAGES 9000
#define NPAGES 57
#define USE_FORK
		Test23,
#ifdef USE_FORK
#else
#endif
#ifdef USE_FORK
#endif
#undef NPAGES
#undef NIMAGES
#endif			
		Test80,
		Test90,
		Exit,
		Quit,
		Menu,
#ifdef DEBUG
		Dbglvl,
#endif
		} COMMAND_CMND ;

typedef struct
		{
		COMMAND_CMND cmnd;
		char *name;
		} COMMAND_LIST ;

#ifdef COMMAND_MAIN

static COMMAND_LIST command_list[] =
		{
		{ T0, "T0" },
		{ Rdt, "Rdt" },
		{ Timestamp, "Timestamp" },
		{ TimeT, "TimeT" },
		{ status, "status" },
		{ reset, "reset" },
		{ Start, "Start" },
		{ Bbar0r, "Bbar0r" },
		{ FullStat, "FullStat" },
		{ FIrqstat, "FIrqstat" },
		{ Ioctl, "Ioctl" },
#define NCYCLES 4096
#define NEXTRAS 10
		{ Pio1, "Pio1" },
#undef NCYCLES
#undef NEXTRAS
#define NPAGES 57
		{ Test18, "Test18" },
#undef NPAGES
#define NTRIES 10000
#define NPAGES 57
		{ Test19, "Test19" },
#undef NPAGES
#undef NTRIES
#define NTRIES 1000
#define NPAGES 57
		{ Test20, "Test20" },
#undef NPAGES
#undef NTRIES
#define NPAGES 57
		{ Test21, "Test21" },
#undef NPAGES
#define NTRIES 3000	
#define NPAGES 57
		{ Test22, "Test22" },
#undef NPAGES
#undef NTRIES
#ifdef _SLS_CAMERA_
#define NIMAGES 9000
#define NPAGES 57
#define USE_FORK
		{ Test23, "Test23" },
#ifdef USE_FORK
#else
#endif
#ifdef USE_FORK
#endif
#undef NPAGES
#undef NIMAGES
#endif			
		{ Test80, "Test80" },
		{ Test90, "Test90" },
		{ Exit, "Exit" },
		{ Quit, "Quit" },
		{ Menu, "Menu" },
#ifdef DEBUG
		{ Dbglvl, "Dbglvl" },
#endif
		} ;

static int command_count = sizeof(command_list)/sizeof(COMMAND_LIST);

/* end of variable data - please do not change this comment */

#endif

#endif
