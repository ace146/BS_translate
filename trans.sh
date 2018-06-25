#!/bin/sh
module="translate"
device="trans"
mode="664"

make clean

# remove module only if its loaded already
if [ $(lsmod | grep $module -c) -eq 1 ]; then
	/sbin/rmmod $module
fi

make

# invoke insmod with all arguments we got
# and use a pathname, as newer modutils don't look in . by default
/sbin/insmod ./$module.ko $* || exit 1

# remove stale nodes
rm -rf /dev/${device}*

major=$(cat /proc/devices | grep -m 1 $device | cut -f 1 -d ' ')

mknod /dev/${device}0 c $major 0
mknod /dev/${device}1 c $major 1

# give appropriate group/permissions, and change the group.
group="users"

chgrp $group /dev/${device}0
chmod $mode  /dev/${device}0
chgrp $group /dev/${device}1
chmod $mode  /dev/${device}1
