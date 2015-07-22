#!/bin/sh

# gs_unload.sh - unload the gs_drv.ko module

module="gsd"
device="gsd"
mntp="`cd ..;pwd`/gsdev"

#umount the gs fs
umount $mntp

# invoke rmmod with arguments from command line
/sbin/rmmod $module $* || exit 1
