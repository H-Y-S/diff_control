// dcb.h - communication with the PILATUS 2 detector control board
// via the PMC GigaSTaR PCI card link
// tabs = 4 to line up columns

#ifndef DCB_H
#define DCB_H

#include "detsys.h"


/*
**  We check the DCB build number as read back
**
**  The basic clock frequency is set in the FPGA design, and gives
**  an underlying clock period, e.g. 40 ns (25 MHz design).
**
**  The exposure timer register (0xfd) shows the upper 32 bits of the 
**  32-bit exposure timer.  Thus 1 lsb in that register represents
**  2^16 * clock_period.
**
**  Build/Version info:
**		 		for single module detectors
**		1414/\!DCB/firmware/config_files/current/Info.txt
**
**				for multi-module detectors with BCB
**		1414/\!BCB/firmware/config_files/current/Info.txt
**		1414/\!BCB/firmware/config_files/current/DCB\ HW\ Version\ 1/Info.txt
**
**    As of 2 Apr 07, the electronics group is using a new versioning policy.
**  Odd version numbers (e.g., 0x1213) are release versions; even numbers
**  are developmental versions.
*/


/*
** Notes:
**
**
*/

/**********************************************************************
DCB - Build Information:  SINGLE module detectors
**********************
1C54: Temp limits 55/15, 45/15, 45/15; Humid limits 80, 80, 30
      respectively on the 3 channels

1AD0: Faster readout sequence, corrected DCB_MIN_READ_TIME_REG

1658: Port to Xilinx tools & new temperature limits

10F8: Corrected 0 hold off problem and added
      lower temperature and humidity thershold.
      (67MHz)
      
0D98: Release Version. Removed subsequent write
      operation after read in non Reduced
      Dynamic Range Mode.
      (67MHz)
      
0D94: Added hold off time register
      that specifies the delay between the end of
      a readout and the beginning of the next
      triggered/external exposure.
      (67MHz)


DCB Build Information:  MULTI module detectors - use with BCB
**********************
1BB8: 30% RH max on Channel 2, mod power rigorously off

1B60: DCB_MIN_READ_TIME_REG is writable, 3 temp sensors,
      15C lower temp limit, 12 bad readings required
	  before tripping.  This was revised from 1B30.

1AC8: Module power up logic back-compatible.

1A3C: Temp limit chan. 2 to 45C

165C: Port to Xilinx tools & new temperature limits

10F9: Removed subsequent write operation after read in
      non Reduced Dynamic Range Mode.
      Added hold off time register that specifies the
      delay between the end of a readout and the
      beginning of the next triggered/external exposure.
      Added lower temperature and humidity thershold.
      (50MHz)
      (use together with BCB build 10F8)

0D30: Debug Version. Extended EXPSR_LIMIT_REG to 32 bits.
      (50MHz)
      (use together with BCB build 0D30)

0A9D: Added "External Multi Exposure Mode"
      (50MHz)
      (use together with BCB build 0A0C(=50MHz))


BCB Build Information:
**********************
1B3C: 100 MHz.  Otherwise like 1AE8

1B40: 75 MHz.  Otherwise like 1AE8

1AE8: 67 MHz, faster readout sequence (use with 165C++)
      MIN_READ_TIME_REG, reference counter

161C: Port to Xilinx tools

10F8: Removed subsequent write operation after read in
      non Reduced Dynamic Range Mode.
      Added hold off time register that specifies the
      delay between the end of a readout and the
      beginning of the next triggered/external exposure.
      (50MHz)
      (use together with DCB build 10F9)

0D30: Debug Version. Extended EXPSR_IN_FRAME_COUNT_REG
      to 32 bits.
      (50MHz)
      (use together with DCB build 0D30)

**********************************************************************/

/*
DCB firmware builds - dcb_firmware_build
  Single module

  	case 0x08c0			// before limo swap, single-module
	case 0x096c:		// old-style fillpix, single-module
	case 0x0a11:		// new-style fillpix, single-module
	case 0x0a9c:		// multi-extt, single-module
	case 0x0d31:		// 32-bit nexpf, single-module
	case 0x0d78:		// improved multi-trigger, single-module
	case 0x0d94:		// enable holdoff, single-module
	case 0x0d98:		// optional parallel write, single-module
	case 0x10f8:		// temp & hdty lower limits, single-module
	case 0x1658:		// uses Xilinx tools, single-module
	case 0x1ad0:		// corrected DCB_MIN_READ_TIME_REG, single-module
	case 0x1c54:		// T: 55/15, 45/15, 45/15, H: 80, 80, 30, single-module

  Multi-module - i.e., with a BCB

	case 0x0a10:		// old, multi-module
	case 0x0a9d:		// multi-extt, multi-module
	case 0x0ca8:		// multi-extt, multi-module
	case 0x0d30:		// 32-bit nexpf, multi-module
	case 0x10f9:		// use with BCB build 10f8, multi-module
	case 0x165c:		// uses Xilinx tools, use with BCB build 0x161c, multi-module
	case 0x1a3c:		// 45C temp limit chan 2, use with BCB build 0x161c, multi-module
	case 0x1ac8:		// 45C limit chan 2, power up enable depends on DIP SW4, multi-module
	case 0x1b60:		// DCB_MIN_READ_TIME_REG R/W, 3 T sensors, 15C lo T lim, 12 bad reads needed, multi-module
	case 0x1bb8:		// 30% RH max on chan. 2

BCB firmware builds - bcb_firmware_build
	case 0x0a0c:      	// 50MHz - use with DCB build 0A10, 0A9D & 0CA8
	case 0x0d30:		// 32-bit nexpf - use with DCB build 0D30
	case 0x10f8:		// use with DCB build 10F9
	case 0x161c:		// uses Xilinx tools - use with DCB build 0x165c
	case 0x1ae8:		// 67 MHz, MIN_READ_TIME_REG, REF_COUNTER - use with DCB build 0x165c++
	case 0x1b3c:		// 100 MHz - use with DCB build 0x165c++
	case 0x1b40:		// 75 MHz - use with DCB build 0x165c++

Temperature preset limit presets:

The upper temperature thresholds are as follows:
DCB single, build 0x1658++, (only 0 and 1 are used):
ch 0 - 55C
ch 1 - 50C
ch 2 - 40C
ch 3 - 45C
ch 4 - 55C
ch 5 - 60C

DCB multi, build 0x1a3c++, (only 0, 1, and 2 are used):
ch 0 - 55C
ch 1 - 35C
ch 2 - 45C
-- after 0x1b60 the following are not present
ch 3 - 40C
ch 4 - 45C
ch 5 - 50C

Lower temperature preset is -39C.
Humidity preset limit is 80%
*/

/*
** A multi-dcb detector can be operated as a collection of independent
** detectors (INDEPENDENT_DCBs to 1), or as a single dector under the
** hardware control of a master dcb (set 0).
*/

#if NMOD_BANK*NBANK > 1
 #define MULTI_MODULE_DETECTOR 1
 #define INDEPENDENT_DCBs 0
// 0x1A for the old readout sequence, 0x1C or 0x1D for the fast sequence
 #define DOUT_SAMPLING_DELAY_1 0x1A
 #define DOUT_SAMPLING_DELAY_2 0x1C
#else							// single module
 #define MULTI_MODULE_DETECTOR 0
#endif


/*
** For testing, do not control DCB build number, but instead assume 
** that the experimental design duplicates the funtionality of the
** given build - use with caution
*/
#if MULTI_MODULE_DETECTOR || defined USE_BCB
//  #define OVERRIDE_DCB_BUILD_CONTROL 0x10f9
//  #define OVERRIDE_BCB_BUILD_CONTROL 0x10f8
#else
//  #define OVERRIDE_DCB_BUILD_CONTROL 0x10f8
#endif

/*
** For testing partially assembled detectors, override strict
** hardware checks by commenting out the next line.
*/
#define STRICT_HARDWARE_CHECKS

#ifdef STRICT_HARDWARE_CHECKS
  # define ERROR_RETURN return -1
#else
  # define ERROR_RETURN
#endif

/*
** In the lab, uncomment the following to override humidity monitor
** at startup - use with caution.
*/
// #define TH_CONTROL_OVERRIDE



//----------------------------------------------------------

/*
** The overhead time determined from the DCB register is empirically 
** a little bit short.  This is seen as one or more bad pixels near,
** but not at, the end of the readout in each chip - 16 bright dots
** across the center line of the module in a blank image.
**
** Extensive debugging with the logic state analyzer shows that this is a
** problem for pixel (7, 96) of each chip; this is the pixel with a spy-pad.
** Adding an extra chip reset at the end of the readout cycle does nothing.
** Symptoms: single exposures are unaffected; the first exposure of a multi-
** exposure is also unaffected.  If there is at least 300-500 us from the 
** end of the readout until ENB goes active again, then there is no artifact.
**
** This has been corrected in the newest chips
**
** 5 Apr 07 - investigations about corrupted readouts when the "dead" time
** is too short.  First problem was power supply - at the end of exposure
** a current spike caused the dcb to make errors.  But, after that was
** worked around, there was still about 200 us when the system should work,
** but the dcb did not deliver the data.  
** So, we still need EXTRA_OVERHEAD_TIME.
** 0x0d98 - external trigger does not work if this is 0
** 0x10f8 - this problem is fixed
** 18Jul08 - not completely fixed for 2M.  Value of 0 is OK, but not backward
** compatible.  But, a value of 1 us, for some reason, blocks external enable,
** whereas a value of 900 ns is OK.  Choose 30 ns.  
** 17 Nov 08 - this is really troublesome.  Some systems require 30 ns
** other systems require 1 us.  No systematic prediction.  
** Fixed in 0x1658, ff.  Use 30 ns.
*/
#define EXTRA_OVERHEAD_TIME_OLD 30.0e-9
#define EXTRA_OVERHEAD_TIME 0.0e-9
#define EXTRA_TRANSFER_TIME 0.0


/*
** commands - these go into bits 24-31 of an instruction; SM = single module, MM = multi-module
*/
#define DCB_I2C_DAC1				(0x11<<24)	// i2c command for dac1 channel
#define DCB_I2C_DAC2				(0x12<<24)	// i2c command for dac2 channel
#define DCB_I2C_CHSEL				(0x13<<24)	// i2c command for chsel channel
#define DCB_TRIGGER_ENABLE			(0x21<<24)	// set enb signal for preset time
#define DCB_STOP					(0x41<<24)	// stop exposure and calpix
#define DCB_EXPOSE					(0x51<<24)	// start exposure sequence - no ACK
#define DCB_SET_IMAGE_TX_ENB		(0x52<<24)	// set image tx enable - no ACK
#define DCB_TRIGGER_AUTO_FIFO_READ	(0x54<<24)	// readstrobe for entire detector - no ACK - MM only
#define DCB_TRIGGER_READSTROBE 		(0x58<<24)	// readstrobe for selected module fifo - no ACK - MM only
#define DCB_INSERT_COL_TOKEN		(0x91<<24)	// insert a column token
#define DCB_INSERT_ROW_TOKEN		(0x92<<24)	// insert a row token
#define DCB_INSERT_ROW_COL_TOKEN	(0x93<<24)	// insert both row & column token
#define DCB_ADVANCE_COL_TOKEN		(0x94<<24)	// advance the column token
#define DCB_ADVANCE_ROW_TOKEN		(0x98<<24)	// advance the row token
#define DCB_ADVANCE_ROW_COL_TOKEN	(0x9c<<24)	// advance both row & col tokens
#define DCB_RESET_ROW_COL			(0xa1<<24)	// reset row and column shift registers
#define DCB_RESET_IMAGE_FIFO		(0xa2<<24)	// reset image fifo
#define DCB_IMAGE_FIFO_ENABLE		(0xa4<<24)	// enable/disable image fifo
#define DCB_TRIGGER_PSEL			(0xb1<<24)	// give psel pulse
#define DCB_TRIGGER_DCAL			(0xc1<<24)	// start train of calibrate pulses
#define DCB_DIN_VALUE				(0xc2<<24)	// load digital value in selected pixels
#define DCB_CALPIX					(0xd2<<24)	// continuously give DCAL pulses
#define DCB_CALPIX_X				(0xd4<<24)	// set MODE & EN for x-ray calpix
#define DCB_READOUT_MODULES			(0xe0<<24)	// trigger readout of all modules - no ACK in SM
#define DCB_WRITE_DATA				(0xe1<<24)	// write data to all pixels
#define DCB_TRIM_ALL_VAL			(0xe2<<24)	// trim all pixels to a value
#define DCB_TRIM_PIXELS				(0xe6<<24)	// trim pixels to individual values
#define DCB_N_CALIBRATE				(0xe8<<24)	// trigger calibrate sequence
#define DCB_READ					(0xfc<<24)	// read the specified register
#define DCB_WRITE					(0xfd<<24)	// write the specified register
#define DCB_SET						(0xfe<<24)	// set bits in the specified register
#define DCB_CLEAR					(0xff<<24)	// reset bits in the specified register


/*
** DCB_READOUT_MODULES (0xe0) is for manual readout - moves data from modules to
**    BCB fifos in the multi-module detector, or from module to host for a single
**    module detector.  Requires module selection in both detector types.
**
** DCB_TRIGGER_READSTROBE (0x58) - reads the selected BCB fifo to the host in the
**    multi-module detector; does nothing in the single module detector
**
** DCB_SET_IMAGE_TX_ENB (0x52) - enables data transmission at the end of an exposure
**    from modules to fifos in the multi-module detector, or from the module to the
**    host in a single module detector.  Does not require module selection.
*/

/*
**  register addresses - these go into bits 16-23 of an instruction
*/
#define BCB_REF_COUNTER_HI_REG		(0x18<<16)	// reference clock high
#define BCB_REF_COUNTER_LOW_REG		(0x19<<16)	// reference clock low
#define BCB_ROI_COL_SET_REG			(0x1a<<16)	// roi column
#define BCB_ROI_ROW_SET_REG			(0x1b<<16)	// roi row
#define BCB__1C__REG				(0x1c<<16)	// reserved
#define BCB_CTRL_REG				(0x30<<16)	// BCB control register for FPGA
#define BCB__31__REG				(0x31<<16)	// reserved
#define BCB_FIFO_STATUS_REG			(0x32<<16)	// BCB external fifo status
#define BCB_LED_REG					(0x33<<16)	// BCB read/write LEDs
#define BCB__35__REG				(0x35<<16)	// reserved
#define BCB__36__REG				(0x36<<16)	// reserved
#define BCB_RESET_REG				(0x37<<16)	// BCB reset
#define BCB_CLOCK_FREQ_REG			(0x38<<16)	// BCB design frequency, MHz
#define BCB_MIN_READ_TIME_REG		(0x3b<<16)	// (holdoff time, clocks)/4
#define BCB_FIRMWARE_BUILD_REG		(0x3e<<16)	// BCB firmware build
#define BCB_VERSION_REG				(0x3f<<16)	// BCB version register
#define DCB_BCB__68__REG			(0x68<<16)	// reserved
#define DCB_BCB_EXPSR_COUNT_REG_LOW	(0x69<<16)	// N exposures actual in frame register (low word)
#define DCB_BCB__6A__REG			(0x6a<<16)	// reserved
#define DCB_BCB__6B__REG			(0x6b<<16)	// reserved
#define DCB_BCB_EXPSR_COUNT_REG_HI	(0x70<<16)	// N exposures actual in frame register (hi word)
#define DCB_BCB_EXPSRS_SNC_READ_HI	(0x71<<16)	// N exposures since last readout (hi word)
#define DCB_BCB_EXPSRS_SNC_READ_LO	(0x72<<16)	// N exposures since last readout (lo word)
#define DCB_BCB_WRITE_DATA_HI_REG	(0x73<<16)	// pixel fill data high nybble
#define DCB_BCB_WRITE_DATA_LOW_REG	(0x74<<16)	// pixel fill data low word
#define DCB_BCB_TRIM_FIFO_IN_REG	(0x75<<16)	// trim value input (6 bits)
#define DCB_BCB_DELAY_REG			(0x76<<16)	// DOUT sampling delay register
#define DCB_BCB__77__REG			(0x77<<16)	// reserved
#define DCB_TEMP_ALERT_THR_REG		(0xd0<<16)	// temp alert threshold
#define DCB_HDTY_ALERT_THR_REG		(0xd1<<16)	// humidity alert threshold
#define DCB_TEMP_ALERT_LOW_THR_REG	(0xd2<<16)	// temp alert lower threshold
#define DCB_HDTY_ALERT_LOW_THR_REG	(0xd3<<16)	// humidity alert lower threshold
#define DCB_TRIG_EN_HOLDOFF_HI_REG	(0xd4<<16)	// min time to hold off enable after trigger, hi byte
#define DCB_TRIG_EN_HOLDOFF_LO_REG	(0xd5<<16)	// min time to hold off enable after trigger, lo byte
#define DCB_DEBOUNCE_TIME_HI_REG	(0xd6<<16)	// minimum enable time high
#define DCB_DEBOUNCE_TIME_LOW_REG	(0xd7<<16)	// minimum enable time low
#define DCB_REF_COUNTER_HI_REG		(0xd8<<16)	// reference clock high
#define DCB_REF_COUNTER_LOW_REG		(0xd9<<16)	// reference clock low
#define DCB_ROI_COL_SET_REG			(0xda<<16)	// ROI column
#define DCB_ROI_ROW_SET_REG			(0xdb<<16)	// ROI row
#define DCB_EXP_LIMIT_REG_HI		(0xdc<<16)	// N exposures to accumulate in frame before readout (hi word)
#define DCB_ENB_TIME_MEAS_HI_REG	(0xdd<<16)	// measured ENB time, high
#define DCB_ENB_TIME_MEAS_MED_REG	(0xde<<16)	// measured ENB time, med
#define DCB_ENB_TIME_MEAS_LOW_REG	(0xdf<<16)	// measured ENB time, low
#define DCB_BANK_ADD_REG			(0xe0<<16)	// bank address register
#define DCB_MOD_ADD_REG				(0xe1<<16)	// module address register
#define DCB__E2__REG				(0xe2<<16)	// reserved
#define DCB_EXP_LIMIT_REG_LOW		(0xe3<<16)	// N exposures to accumulate in frame before readout (low word)
#define DCB_TX_FR_LIM_REG			(0xe4<<16)	// number of frames to transmit (0 = continuous)
#define DCB_TX_FR_COUNT_REG			(0xe5<<16)	// number of frames actually transmitted
#define DCB_EXP_TRIG_DELAY_HI_REG	(0xe6<<16)	// delay from ext trigger high word (16-bit) register
#define DCB_EXP_TRIG_DELAY_LOW_REG	(0xe7<<16)	// delay from ext trigger low word (16-bit) register
#define DCB_EXP_TIME_HI_REG			(0xe8<<16)	// exposure time high word (16-bit) register
#define DCB_EXP_TIME_MED_REG		(0xe9<<16)	// exposure time mid word (16-bit) register
#define DCB_EXP_TIME_LOW_REG		(0xea<<16)	// exposure time low word (16-bit) register
#define DCB_EXPSR_TIMER_HI_REG		(0xeb<<16)	// remaining time in exposure (top 16 bits)
#define DCB_EXPSR_TIMER_LOW_REG		(0xec<<16)	// remaining time in exposure (mid 16 bits)
#define DCB_EXP_PERIOD_HI_REG		(0xed<<16)	// exposure period high word (16-bit) register
#define DCB_EXP_PERIOD_MED_REG		(0xee<<16)	// exposure period mid word (16-bit) register
#define DCB_EXP_PERIOD_LOW_REG		(0xef<<16)	// exposure period low word (16-bit) register
#define DCB_CTRL_REG				(0xf0<<16)	// control register for FPGA
#define DCB_STATUS_REG				(0xf1<<16)	// status register for DCB
#define DCB_FIFO_STATUS_REG			(0xf2<<16)	// image & external fifo status register
#define DCB_LED_REG					(0xf3<<16)	// LED register
#define DCB_DIP_SW_REG				(0xf4<<16)	// dip switch register
#define DCB_BANK_ENABLE_REG			(0xf5<<16)	// pattern of banks to enable
#define DCB_MODULE_ENABLE_REG		(0xf6<<16)	// modules to enable on each bank
#define DCB_RESET_REG				(0xf7<<16)	// reset register
#define DCB_CLOCK_FREQ_REG			(0xf8<<16)	// DCB design frequency, MHz
#define DCB_HALF_DWORD_REG			(0xf9<<16)	// half dword register
#define DCB_T_H_SENSOR_STATUS_REG	(0Xfa<<16)	// temp/humid sensor control
#define DCB_MIN_READ_TIME_REG		(0xfb<<16)	// (holdoff time, clocks)/4
#define DCB_TEMPERATURE_REG			(0xfc<<16)	// temperature sensor register
#define DCB_HUMIDITY_REG			(0xfd<<16)	// humidity sensor register
#define DCB_FIRMWARE_BUILD_REG		(0xfe<<16)	// firmware build
#define DCB_VERSION_REG				(0xff<<16)	// version register


/*
** bit masks for the BCB control register (0x30)
*/
#define BCB_REDUCED_DYNAMIC_RANGE	(1<<0)	// 8-bit (0) / 4-bit (1) dynamic range
#define BCB_REDUCED_DYN_RANGE_ENB	(1<<1)	//
#define BCB_DELAY_SELECT			(1<<2)	//
#define BCB_REF_COUNTER_ENB			(1<<12)	//

/*
** bit masks for the BCB fifo status register (0x32)
*/
#define BCB_TRIM_FIFO_EMPTY				(1<<6)		//
#define BCB_TRIM_FIFO_FULL				(1<<7)		//
#define BCB_EXT_FIFO_EMPTY				(1<<8)		//
#define BCB_EXT_FIFO_ALMOST_EMPTY		(1<<9)		//
#define BCB_EXT_FIFO_HALF_FULL			(1<<10)		//
#define BCB_EXT_FIFO_ALMOST_FULL		(1<<11)		//
#define BCB_EXT_FIFO_FULL				(1<<12)		//
#define BCB_EXT_FIFO_UNDERFLOW			(1<<14)		//
#define BCB_EXT_FIFO_IMG_OVERFLOW		(1<<15)		//

/*
** bit masks for the BCB LED register (0x33)
*/
#define BCB_DCM_LOCK					(1<<0)		// LED 1 - blue
#define BCB_TRIM_FIFO_FULL_LED			(1<<1)		//
#define BCB_EXT_FIFO_UNDERFLOW_LED		(1<<2)		//
#define BCB_EXT_FIFO_IMG_OVERFLOW_LED	(1<<3)		//

/*
** bit masks for the BCB reset register (0x37)
*/
#define BCB_REF_COUNTER_RESET			(1<<0)	// 
#define BCB_EXT_FIFO_MASTER_RESET		(1<<4)	//
#define BCB_EXT_FIFO_PARTIAL_RESET		(1<<5)	//
#define BCB_EXT_FIFO_SPEC_FLAGS_RESET	(1<<6)	//
#define BCB_SEQUENCE_GEN_RESET			(1<<7)	//
#define BCB_RESET_ALL					(1<<15)	//

/*
** bit masks for the BCB firmwre build register (0x3e)
*/
#define BCB_FIRMWARE_BUILD_RUN			(3<<0)		// 2 bits
#define BCB_FIRMWARE_BUILD_DAY			(0x1f<<2)	// 5 bits
#define BCB_FIRMWARE_BUILD_MONTH		(0xf<<7)	// 4 bits
#define BCB_FIRMWARE_BUILD_YEAR			(0x1f<12)	// 5 bits

/*
** bit masks for the BCB version register (0x3f)
*/
#define BCB_FIRMWARE_MINOR			(0xf<<0)	// 4 bits
#define BCB_FIRMWARE_MAJOR			(0xf<<4)	// 4 bits
#define BCB_FIRMWARE_LEVEL			(0xf<<8)	// 4 bits

/*
** bit masks for the exposures in frame count register (0x69)
*/

/*
** bit masks for the delay register (0x76)
*/
#define DCB_SW_DELAY				(0xff<<0)	// software delay, 8 bits
#define DCB_HW_DELAY				(0xff<<8)	// hardware delay, 8 bits

/*
** bit masks for the control register (0xF0)
*/
#define DCB_REDUCED_DYNAMIC_RANGE	(1<<0)		// 8-bit (0) / 4-bit (1) dynamic range
#define DCB_REDUCED_DYN_RANGE_ENB	(1<<1)		// enable (1) reduced dynamic range mode
#define DCB_DELAY_SELECT			(1<<2)		// sw (0) / hw (1) delay value select; dflt=1
#define DCB_ENABLE_IMAGE_TX			(1<<3)		// enable (1) image transmission
#define DCB_ENB_MULTI_EXT_TRIGGER	(1<<4)		// enable multi-external trigger
#define DCB_EXT_EXP_DEBOUNCE_ENB	(1<<5)		// enable (1) the debounce timer
#define DCB_EXPOSURE_CONTROL		(1<<6)		// internal (0) / external (1) ENB
#define DCB_EXT_TRIGGER_CONTROL		(1<<7)		// ext expose trigger off (0) / on (1)
#define DCB_SHT_SENSOR_HEATER		(1<<8)		// T/H sensor heater off (0) / on (1)
#define DCB_SHT_SENSOR_RESOL		(1<<9)		// T/H sensor resolution hi (0) / lo (1)
#define DCB_EXT_TRIGGER_MODE		(1<<10)		// sequences (0) / exposures (1)
#define DCB_REF_COUNTER_ENB			(1<<12)		// enable (1) the reference counter
#define DCB_MODULE_POWER_UP			(1<<13)		// force module power down (0) / up (1)

/*
** bit masks for the status register (0xF1)
*/
#define DCB_RX_LOCK					(1<<0)		//
#define DCB_RX_SYNC					(1<<1)		// inverted
#define DCB_RX_PARITY_ERROR			(1<<2)		//
#define DCB_IMAGE_TX_ENABLED		(1<<3)		//
#define DCB_RX_FIFO_EMPTY			(1<<4)		//
#define DCB_RX_FIFO_ALMOST_EMPTY	(1<<5)		//
#define DCB_RX_FIFO_ALMOST_FULL		(1<<6)		//
#define DCB_RX_FIFO_FULL			(1<<7)		//
#define DCB_TX_LOCK					(1<<8)		//
#define DCB_ENABLE_SIGNAL_STATE		(1<<11)		// read enable signal state
#define DCB_DCB_TYPE				(1<<12)		// single (0) / multi-mod (1)
#define DCB_SHT_SENSOR_CRC_ERR		(1<<14)		// T/H sensor CRC error
#define DCB_SHT_SENSOR_TX_ERR		(1<<15)		// T/H sensor TX error

/*
** bit masks for the internal fifo status reg (0xF2)
*/
#define DCB_INT_FIFO_EMPTY			(1<<0)		//
#define DCB_INT_FIFO_ALMOST_EMPTY	(1<<1)		//
#define DCB_INT_FIFO_ALMOST_FULL	(1<<2)		//
#define DCB_INT_FIFO_FULL			(1<<3)		//
#define DCB_EXT_FIFO_EMPTY			(1<<8)		// inverted
#define DCB_EXT_FIFO_ALMOST_EMPTY	(1<<9)		// inverted
#define DCB_EXT_FIFO_HALF			(1<<10)		// inverted
#define DCB_EXT_FIFO_ALMOST_FULL	(1<<11)		// inverted
#define DCB_EXT_FIFO_FULL			(1<<12)		// inverted

/*
** bit masks for the DCB LED register (0xF3)
*/
#define DCB_DCM_LOCK_LED				(1<<0)		// blue 1
#define DCB_IMG_TX_ENABLED_LED			(1<<1)		// blue 2
#define DCB_TRIM_FIFO_FULL_LED			(1<<2)		// blue 3
#define DCB_TRIM_FIFO_EMPTY_LED			(1<<3)		// blue 4
#define DCB_IMG_FIFO_EMPTY_LED			(1<<4)		// green 1
#define DCB_IMG_FIFO_ALMOST_EMPTY_LED	(1<<5)		// green 2
#define DCB_IMG_FIFO_ALMOST_FULL_LED	(1<<6)		// green 3
#define DCB_IMG_FIFO_FULL_LED			(1<<7)		// green 4
#define DCB_DCB_TYPE_LED				(1<<8)		// yellow 1 - single (0) / multi-mod (1)
#define DCB_HW_FW_VER_MISMATCH_LED		(1<<9)		// yellow 2
#define DCB_SW_HW_DELAY_VAL_SEL_LED		(1<<10)		// yellow 3 = sw(0)/hw(1) delay value select
#define DCB_IMG_TX_ENB_LOCAL_LED		(1<<11)		// yellow 4 = enable image transmission
#define DCB_SHT_SENSOR_HTR_LED			(1<<12)		// red 1 ) = temp sensor address
#define DCB_SHT_RESOLUTION_LED			(1<<13)		// red 2 )
#define DCB_SHT_SENSOR_CRC_ERR_LED		(1<<14)		// red 3 ) = humidity sensor address
#define DCB_SHT_SENSOR_TX_ERR_LED		(1<<15)		// red 4 )

/*
** bit masks for the dip switch register (0xF4)
*/
#define DCB_REPEATER_MODE_SW		(1<<0)		// sw 1
#define DCB_LED_ENB_SW				(1<<1)		// sw 2
#define DCB_DEBUG_ENB_SW			(1<<2)		// sw 3
#define DCB_PWR_BOARD_RMT_ENB_SW	(1<<3)		// sw 4
#define DCB_GIGASTAR_EQU_ENB_SW		(1<<4)		// sw 5
#define DCB_WRITE_LOOP_ENB_SW		(1<<5)		// sw 6
#define DCB_CALIBRATE_LOOP_ENB_SW	(1<<6)		// sw 7
#define DCB_GSTAR_RX_RESET_SW		(1<<7)		// sw 8
#define DCB_CONTROLLER_RESET_SW		(1<<8)		// sw 9
#define DCB_DCM_RESET_SW			(1<<9)		// sw 10

/*
** bit masks for the reset register (0xF7)
*/
#define DCB_GSTAR_TX_RESET			(1<<0)		//
#define DCB_GSTAR_RX_RESET			(1<<1)		//
#define DCB_TX_FIFO_RESET			(1<<2)		//
#define DCB_RX_FIFO_RESET			(1<<3)		//
#define DCB_REF_COUNTER_RESET		(1<<4)		//
#define DCB_INT_FIFO_RESET			(1<<6)		//
#define DCB_SEQUENCE_GEN_RESET		(1<<7)		// 
#define DCB_CONVERT_RESET			(1<<8)		// 32 to 16 bit conversion reset
#define DCB_TRIM_FIFO_RESET			(1<<9)		//
#define DCB_FRAME_COUNT_RESET		(1<<10)		// frame count register reset
#define DCB_BOARD_COMM_RESET		(1<<11)		// board communication reset
#define DCB_SHT_SOFT_RESET			(1<<12)		// temp/humid sensor full reset
#define DCB_SHT_COMM_RESET			(1<<13)		// temp/humid communication reset
#define DCB_AUTO_FIFO_READ_RESET	(1<<14)		//
#define DCB_RESET_ALL				(1<<15)		//


/*
** bit masks for the firmware build register (0xFE)
*/
#define DCB_FIRMWARE_BUILD_RUN		(3<<0)		// 2 bits
#define DCB_FIRMWARE_BUILD_DAY		(0x1f<<2)	// 5 bits
#define DCB_FIRMWARE_BUILD_MONTH	(0xf<<7)	// 4 bits
#define DCB_FIRMWARE_BUILD_YEAR		(0x1f<12)	// 5 bits


/*
** bit masks for the version register (0xFF)
*/
#define DCB_FIRMWARE_MINOR			(0xf<<0)	// 4 bits
#define DCB_FIRMWARE_MAJOR			(0xf<<4)	// 4 bits
#define DCB_FIRMWARE_LEVEL			(0xf<<8)	// 4 bits
#define DCB_HARDWARE_VERSION		(0xf<<12)	// 4 bits


#endif
