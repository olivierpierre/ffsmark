#!/bin/sh

# Path to flashmon.ko compiled for the targetted system
FLASHMON="flashmon.ko"
###########################################################
# WARNING # WARNING # WARNING # WARNING # WARNING # WARNING 
# Target partition - WARNING : it will be totally erased !!
# Please make sure this is a test partition and not your
# rootfs / kernel / bootloader or anything important !
TARGET="/dev/mtd6"
# WARNING # WARNING # WARNING # WARNING # WARNING # WARNING
###########################################################
# Size to write / read
SIZE_MB=5

if [ ! `whoami` == "root" ]; then
	echo "Please run this script as root"
	exit
fi

if [ ! "$1" == "-y" ]; then
	echo "Warning !!! The entirety of $TARGET will be erased during this test"
	echo "Please make sure this is a test partition and not your rootfs /" 
	echo "kernel / bootloader or anything important !"
	echo "Please type 'yes' to proceed ..."
	read x

	if [ ! "$x" == "yes" ]; then
		echo "Aborting."
		exit
	fi
fi

MODULE_NAME=`basename $FLASHMON`

# 1. remove flashmon if still loaded
rmmod $MODULE_NAME &> /dev/null
# Test module
insmod $FLASHMON
if [ ! -e "/proc/flashmon" ]; then
	echo "Error loading flashmon ..."
	exit
fi
rmmod $MODULE_NAME

# 2. erase partition
echo "======== Erase Test ======================================="
echo "erase test without flashmon :"
echo 3 > /proc/sys/vm/drop_caches
time flash_erase $TARGET 0 0 -q
insmod $FLASHMON
echo "erase test with flashmon :"
echo 3 > /proc/sys/vm/drop_caches
time flash_erase $TARGET 0 0 -q
rmmod $MODULE_NAME

# 3. Write test
dd if=/dev/urandom of=./test.dat bs=1M count=$SIZE_MB &> /dev/null

echo "======== Write Test ======================================="
echo "write test without flashmon :"
echo 3 > /proc/sys/vm/drop_caches
time nandwrite $TARGET test.dat -p -q
flash_erase $TARGET 0 0 -q
insmod $FLASHMON
echo "write test with flashmon :"
echo 3 > /proc/sys/vm/drop_caches
time nandwrite $TARGET test.dat -p -q
rmmod $MODULE_NAME

# 4. Read test
echo "======== Read Test ======================================="
rm test.dat
echo "read test without flashmon"
let size="$SIZE_MB*1024*1024"
echo 3 > /proc/sys/vm/drop_caches
time nanddump $TARGET -l $size -f /dev/null -q
insmod $FLASHMON
echo "read test with flashmon"
echo 3 > /proc/sys/vm/drop_caches
time nanddump $TARGET -l $size -f /dev/null -q
rmmod $MODULE_NAME
echo ""
echo "NOTE : you may want to run this script several times and compute"
echo "mean values for the difference between time with and without flashmon"
