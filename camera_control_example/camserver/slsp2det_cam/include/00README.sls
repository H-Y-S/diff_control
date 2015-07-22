Include files for the sls detector systems

tvxcampkg.h - a bundle of include files to be included by camserver.c
tvxcampkg_decl.h - declarations of variables for inclusion in camserver.c

detsys.h - overall (top level) configuration of the system
-- use this to turn on/off global configuration options

detsys.h is divided into standard configurations that are selected
by uncommenting the appropriate line

cam_tbl.h - enums for cam_tble.c (a dynamically configured file)
pix_detector.h - variables and signals for the pixel detector
ppg_util.h - declarations for the pattern generator and fifo

The following are for the gpib oscilloscope interface
vxi11.h
vxi11core.h
vxi11intr.h


============================= NOTE ===========================
Camera parameters must be configured in:

tvx/slsp2det/include/tvxpkg.h
tvx/camera/camserver/slsp2det_cam/gstar/src/gs_drv/gsd.h

and in (e.g., if your camera directory is ~/p2_det):
  ~/p2_det/tvxrc
  ~/p2_det/config/cam_data/camera.def

cf. 00README.sls in
tvx/slsp2det/include



