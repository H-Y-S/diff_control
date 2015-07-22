#!/bin/sh

# gs_load.sh - load the gsd.ko module
# must be root

module="gsd"
device="gsd"
mode="a+rwX"
mntp="`cd ..;pwd`/gsdev"

# invoke insmod with arguments given on command line
/sbin/insmod ./$module.ko $* || exit 1

# mount the new fs type and adjust permissions
# create the mount point if needed
cd ..
if ! test -d $mntp; then
	mkdir $mntp
fi
mount -t gsfs none $mntp
chmod -R $mode $mntp
