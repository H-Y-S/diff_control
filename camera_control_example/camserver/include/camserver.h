/* camserver.h - definitions of enums and structures for camserver.c
*/


// Note that this header allocates space, something you should not do in a
// header file.  But, this is a one-use only header, so this doesn't cause
// problems.

#ifndef CAMSERVER_H
#define CAMSERVER_H

#define LENFN 320

#ifndef True
#define True 1
#define False 0
#endif

#define CAMWAIT 500

extern int timerRunning;

/* see camserver_casedefs.txt to assign values to the enums */

/* start of CAMERA enum - please do not change this comment - lines below will change */
typedef enum {
		CamCmd = 1,
		CamSetup = 2,
		CamWait = 3,
		DataPath = 4,
		Df = 5,
		ExpEnd = 6,
		Exposure = 7,
		ExpTime = 8,
		HeaderString = 9,
		ImgPath = 10,
		LdCmndFile = 11,
		Read_setup = 12,
		K = 13,
		ResetCam = 14,
		Send = 15,
		ShowPID = 16,
		ShutterEnable = 17,
		TelemetrY = 18,
		ExiT = 19,
		QuiT = 20,
		MenU = 21,
		Status = 22,
		CamStatus,
		SeT = 200,
#ifdef SLAVE_COMPUTER
#endif
		Reset = 201,
		ProG = 202,
#ifdef SLAVE_COMPUTER
#endif
#ifdef MASTER_COMPUTER
#endif
		Show = 203,
		Load = 210,
#ifdef MASTER_COMPUTER
#else
#endif
		Save = 211,
		THread = 215,
#ifdef MASTER_COMPUTER
#else
#endif
#ifdef MASTER_COMPUTER
#else
#endif
		ImgonlY,
		Cpix,
		Cpix_x,
		Stop,
		Fillpix,
		SFillpix,
		Calibrate,
#ifdef SLSP2DET_CAMERA
		Trim,
#else
		Trim,
#endif		
		Trim_all,
		SetTrims,
		TrimFromFile,
		LogImgFile,
		Read_signals,
		LogSettings,
		ImgMode,
#ifdef USE_GPIB
		Gpibsend_volt,
		Gpibread_volt,
		Gpibread_scope,
		Gpibsend_scope,
		Scopeimg,
#else
#endif		
		NExpFrame,
		NFrameImg,
		NImages,
		ExpPeriod,
		ExtEnable,
		ExtTrigger,
		ExtMTrigger,
		Delay,
#if defined SLAVE_COMPUTER
#endif
#if defined MASTER_COMPUTER
#endif
		DebTime,
		DataSection,
		VDelta,
		Tau,
		SensorHtr,
		SetLimTH,
		SetSDelay,
		BitMode,
		Dcb_init,
		GapFill,
#ifdef SLSP2_1M_CAMERA
#endif
		DiscardMultIm,
		DacOffset,
		MXsettings,
		CalibFile,
		SetThreshold,
		LdBadPixMap,
		LdFlatField,
		SetAckInt,
		SetEnergy,
		ScanDAC,
		ExpNaming,
		Cam_start,
#if 0
		ETest,
		PxFill,
		PxF2,
		PxF4,
		PxF5,
#else
#endif
#ifdef DEBUG
		CamNoop,
		DbglvL,
#endif
		} CAMERA_CMND ;

typedef struct
		{
		CAMERA_CMND cmnd;
		char *name;
		} CAMERA_LIST ;

#ifdef CAMERA_MAIN

static CAMERA_LIST camera_list[] =
		{
		{ CamCmd, "CamCmd" },
		{ CamSetup, "CamSetup" },
		{ CamWait, "CamWait" },
		{ DataPath, "DataPath" },
		{ Df, "Df" },
		{ ExpEnd, "ExpEnd" },
		{ Exposure, "Exposure" },
		{ ExpTime, "ExpTime" },
		{ HeaderString, "HeaderString" },
		{ ImgPath, "ImgPath" },
		{ LdCmndFile, "LdCmndFile" },
		{ Read_setup, "Read_setup" },
		{ K, "K" },
		{ ResetCam, "ResetCam" },
		{ Send, "Send" },
		{ ShowPID, "ShowPID" },
		{ ShutterEnable, "ShutterEnable" },
		{ TelemetrY, "TelemetrY" },
		{ ExiT, "ExiT" },
		{ QuiT, "QuiT" },
		{ MenU, "MenU" },
		{ Status, "Status" },
		{ CamStatus, "CamStatus" },
		{ SeT, "SeT" },
#ifdef SLAVE_COMPUTER
#endif
		{ Reset, "Reset" },
		{ ProG, "ProG" },
#ifdef SLAVE_COMPUTER
#endif
#ifdef MASTER_COMPUTER
#endif
		{ Show, "Show" },
		{ Load, "Load" },
#ifdef MASTER_COMPUTER
#else
#endif
		{ Save, "Save" },
		{ THread, "THread" },
#ifdef MASTER_COMPUTER
#else
#endif
#ifdef MASTER_COMPUTER
#else
#endif
		{ ImgonlY, "ImgonlY" },
		{ Cpix, "Cpix" },
		{ Cpix_x, "Cpix_x" },
		{ Stop, "Stop" },
		{ Fillpix, "Fillpix" },
		{ SFillpix, "SFillpix" },
		{ Calibrate, "Calibrate" },
#ifdef SLSP2DET_CAMERA
		{ Trim, "Trim" },
#else
		{ Trim, "Trim" },
#endif		
		{ Trim_all, "Trim_all" },
		{ SetTrims, "SetTrims" },
		{ TrimFromFile, "TrimFromFile" },
		{ LogImgFile, "LogImgFile" },
		{ Read_signals, "Read_signals" },
		{ LogSettings, "LogSettings" },
		{ ImgMode, "ImgMode" },
#ifdef USE_GPIB
		{ Gpibsend_volt, "Gpibsend_volt" },
		{ Gpibread_volt, "Gpibread_volt" },
		{ Gpibread_scope, "Gpibread_scope" },
		{ Gpibsend_scope, "Gpibsend_scope" },
		{ Scopeimg, "Scopeimg" },
#else
#endif		
		{ NExpFrame, "NExpFrame" },
		{ NFrameImg, "NFrameImg" },
		{ NImages, "NImages" },
		{ ExpPeriod, "ExpPeriod" },
		{ ExtEnable, "ExtEnable" },
		{ ExtTrigger, "ExtTrigger" },
		{ ExtMTrigger, "ExtMTrigger" },
		{ Delay, "Delay" },
#if defined SLAVE_COMPUTER
#endif
#if defined MASTER_COMPUTER
#endif
		{ DebTime, "DebTime" },
		{ DataSection, "DataSection" },
		{ VDelta, "VDelta" },
		{ Tau, "Tau" },
		{ SensorHtr, "SensorHtr" },
		{ SetLimTH, "SetLimTH" },
		{ SetSDelay, "SetSDelay" },
		{ BitMode, "BitMode" },
		{ Dcb_init, "Dcb_init" },
		{ GapFill, "GapFill" },
#ifdef SLSP2_1M_CAMERA
#endif
		{ DiscardMultIm, "DiscardMultIm" },
		{ DacOffset, "DacOffset" },
		{ MXsettings, "MXsettings" },
		{ CalibFile, "CalibFile" },
		{ SetThreshold, "SetThreshold" },
		{ LdBadPixMap, "LdBadPixMap" },
		{ LdFlatField, "LdFlatField" },
		{ SetAckInt, "SetAckInt" },
		{ SetEnergy, "SetEnergy" },
		{ ScanDAC, "ScanDAC" },
		{ ExpNaming, "ExpNaming" },
		{ Cam_start, "Cam_start" },
#if 0
		{ ETest, "ETest" },
		{ PxFill, "PxFill" },
		{ PxF2, "PxF2" },
		{ PxF4, "PxF4" },
		{ PxF5, "PxF5" },
#else
#endif
#ifdef DEBUG
		{ CamNoop, "CamNoop" },
		{ DbglvL, "DbglvL" },
#endif
		} ;

static int camera_count = sizeof(camera_list)/sizeof(CAMERA_LIST);

/* end of variable data - please do not change this comment */

#endif

#endif
