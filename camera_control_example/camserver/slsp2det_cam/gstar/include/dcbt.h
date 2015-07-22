/* commando.h - definitions of enums and structures for commando.c
**
** This file is dynamically updated during compilation.  User header info
** may be safely placed up to the line "start of COMMAND enum..."  Lines
** below that will be altered.
*/

#ifndef DCBT_H
#define DCBT_H

// user header info can be put here - it will be preserved

#define TextBufferLength 500
#define LINELNG TextBufferLength
#define MaxRows 1
#define True 1
#define False 0

int xor_tab_initialize(int);


#undef COMMAND		/* will be redefined below */

/* start of COMMAND enum - please do not change this comment - lines below will change */
typedef enum {
		DMA_time = 1,
		TT1,
		TT2,
		TT3,
		TT4,
		TT5,
		Stop,
		TT6,
		Link_test,
		Reset,
#if 0
#endif
		GSreset,
		sGSreset,
		Version,
		Runout,
#define NINTS 10
		Test11,
#undef NINTS
		ReadControl,
		ReadBCBControl,
		SetHWdelay,
		WriteControl,
		Status,
		FStatus,
		GSstatus,
		ReadLED,
		ReadDS,
#if 0		
		ReadDSE,
		WriteDSE,
#endif
		ReadDelay,
		WriteDelay,
		ReadHWR,
		ReadTR,
		ReadHR,
		Fill,
		Rdout,
		Img,
#if 0
#endif
#if 0
#endif
#if 1
#endif
		I2,
		RegT,
		ExpT,
		Exposure,
		Adds,
		SelBM,
		DVersion,
		BVersion,
		FrameCount,
		BCBreset,
		Loop,
		Trim,
		TrmAll,
		Cfill,
		T20,
		T21,
		T22,
		T23,
		T24,
#if 0
#endif
		T25,
		T27,
#if 0
#endif
		T28,
		T29,
		Exp2,
		Rd2,
		Rd3,
		Rd4,
		Exp4,
		Timeleft,
		PDAC,
		SetDACs,
		PCHSEL,
		SelAll,
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
		{ DMA_time, "DMA_time" },
		{ TT1, "TT1" },
		{ TT2, "TT2" },
		{ TT3, "TT3" },
		{ TT4, "TT4" },
		{ TT5, "TT5" },
		{ Stop, "Stop" },
		{ TT6, "TT6" },
		{ Link_test, "Link_test" },
		{ Reset, "Reset" },
#if 0
#endif
		{ GSreset, "GSreset" },
		{ sGSreset, "sGSreset" },
		{ Version, "Version" },
		{ Runout, "Runout" },
#define NINTS 10
		{ Test11, "Test11" },
#undef NINTS
		{ ReadControl, "ReadControl" },
		{ ReadBCBControl, "ReadBCBControl" },
		{ SetHWdelay, "SetHWdelay" },
		{ WriteControl, "WriteControl" },
		{ Status, "Status" },
		{ FStatus, "FStatus" },
		{ GSstatus, "GSstatus" },
		{ ReadLED, "ReadLED" },
		{ ReadDS, "ReadDS" },
#if 0		
		{ ReadDSE, "ReadDSE" },
		{ WriteDSE, "WriteDSE" },
#endif
		{ ReadDelay, "ReadDelay" },
		{ WriteDelay, "WriteDelay" },
		{ ReadHWR, "ReadHWR" },
		{ ReadTR, "ReadTR" },
		{ ReadHR, "ReadHR" },
		{ Fill, "Fill" },
		{ Rdout, "Rdout" },
		{ Img, "Img" },
#if 0
#endif
#if 0
#endif
#if 1
#endif
		{ I2, "I2" },
		{ RegT, "RegT" },
		{ ExpT, "ExpT" },
		{ Exposure, "Exposure" },
		{ Adds, "Adds" },
		{ SelBM, "SelBM" },
		{ DVersion, "DVersion" },
		{ BVersion, "BVersion" },
		{ FrameCount, "FrameCount" },
		{ BCBreset, "BCBreset" },
		{ Loop, "Loop" },
		{ Trim, "Trim" },
		{ TrmAll, "TrmAll" },
		{ Cfill, "Cfill" },
		{ T20, "T20" },
		{ T21, "T21" },
		{ T22, "T22" },
		{ T23, "T23" },
		{ T24, "T24" },
#if 0
#endif
		{ T25, "T25" },
		{ T27, "T27" },
#if 0
#endif
		{ T28, "T28" },
		{ T29, "T29" },
		{ Exp2, "Exp2" },
		{ Rd2, "Rd2" },
		{ Rd3, "Rd3" },
		{ Rd4, "Rd4" },
		{ Exp4, "Exp4" },
		{ Timeleft, "Timeleft" },
		{ PDAC, "PDAC" },
		{ SetDACs, "SetDACs" },
		{ PCHSEL, "PCHSEL" },
		{ SelAll, "SelAll" },
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
