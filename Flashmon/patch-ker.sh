#!/bin/sh

#~ FLASHMON flash memory monitoring tool (Version 2.1)
#~ Revision Authors: Pierre Olivier<pierre.olivier@univ-ubs.fr>, Jalil Boukhobza <boukhobza@univ-brest.fr>
#~ Contributors: Pierre Olivier, Ilyes Khetib
#~ 
#~ Copyright (c) of University of Occidental Britanny (UBO) <boukhobza@univ-brest.fr>, 2010-2013.
#~ 
#~ This program is free software: you can redistribute it and/or modify
#~ it under the terms of the GNU General Public License as published by
#~ the Free Software Foundation, either version 3 of the License, or
#~ (at your option) any later version.
 #~ 
#~ NO WARRANTY. THIS SOFTWARE IS FURNISHED ON AN "AS IS" BASIS.
#~ UNIVERSITY OF OCCIDENTAL BRITANNY MAKES NO WARRANTIES OF ANY KIND, EITHER
#~ EXPRESSED OR IMPLIED AS TO THE MATTER INCLUDING, BUT NOT LIMITED
#~ TO: WARRANTY OF FITNESS FOR PURPOSE OR MERCHANTABILITY, EXCLUSIVITY
#~ OF RESULTS OR RESULTS OBTAINED FROM USE OF THIS SOFTWARE. 
#~ See the GNU General Public License for more details.
#~ 
#~ You should have received a copy of the GNU General Public License
#~ along with this program.  If not, see <http://www.gnu.org/licenses/>.

# This is _widely_ inspired by the yaffs2 kernel patch see <http://www.yaffs.net/>
# Usage :
# ./patch-ker.sh c/l kernel_path
# c is copy, l is link

COPYORLINK=$1
LINUXDIR=$2

# Display usage of this script
usage () {
	echo "usage:  $0  c/l kernelpath"
	echo " if c/l is c, then copy. If l then link"
	exit 1
}

# No linux dir ?
if [ -z $LINUXDIR ]
then
    usage;
fi

# Copy or link source file ?
if [ $COPYORLINK = l ]; then
   CPY="ln -s"
elif [ $COPYORLINK = c ]; then
   CPY="cp"
else
   echo "unknown copy or link type"
   usage;
fi

# Check if kerneldir contains a Makefile
if [ ! -f $LINUXDIR/Makefile ]
then
	echo "Directory $LINUXDIR does not exist or is not a kernel source directory"
	echo " (at least it does not contain a Makefile)"
	exit 1;
fi

# Get kernel version
VERSION=`grep -s VERSION <$LINUXDIR/Makefile | head -n 1 | sed s/'VERSION = '//`
PATCHLEVEL=`grep -s PATCHLEVEL <$LINUXDIR/Makefile | head -n 1 | sed s/'PATCHLEVEL = '//`
SUBLEVEL=`grep -s SUBLEVEL <$LINUXDIR/Makefile | head -n 1 | sed s/'SUBLEVEL = '//`

# TODO : we have to determine a _minimum_ version of Linux to be 
# supported. This needs testing on various linux versions !!

KCONFIG=$LINUXDIR/drivers/mtd/nand/Kconfig
KCONFIGOLD=$LINUXDIR/drivers/mtd/nand/Kconfig.pre.flashmon
FLASHMON_PATCHED_STRING=`grep -s FLASHMON <$KCONFIG | head -n 1`

MAKEFILE=$LINUXDIR/drivers/mtd/nand/Makefile
MAKEFILEOLD=$LINUXDIR/drivers/mtd/nand/Makefile.pre.flashmon

# Kconfig
if [ ! -z "$FLASHMON_PATCHED_STRING" ]
then
    FLASHMON_PATCHED=0
    echo "$KCONFIG already mentions flashmon, so we will not change it"
else
	# Backup old Kconfig
	cp $KCONFIG $KCONFIGOLD
	# insertion into Kconfig
	sed -e "/MTD_NAND_NANDSIM/i\
	config MTD_NAND_FLASHMON\n\ttristate \"Support for Flashmon NAND Flash Operations Tracer\"\n\tdepends on MTD_NAND\n\tdefault n\n\tselect KPROBES\n
	" $KCONFIGOLD > $KCONFIG
	
	sed -i -e "/MTD_NAND_NANDSIM/i\
	config MTD_NAND_FLASHMON_LOG\n\tint \"Flashmon Log Default Size\"\n\tdepends on MTD_NAND_FLASHMON\n\tdefault 1024\n
	" $KCONFIG
	
fi

# Makefile
FLASHMON_PATCHED_STRING=`grep -s flashmon <$MAKEFILE | head -n 1`

if [ ! -z "$FLASHMON_PATCHED_STRING" ]
then
	echo "$MAKEFILE already mentions flashmon, so we will not change it"
else
	# Backup old Makefile
	cp $MAKEFILE $MAKEFILEOLD
	# insertion into Makefile
	# Now here the line where we insert flashmon.o is very important. Actually
	# if flashmon.o is placed before the object file corresponding to the nand
	# driver (mxc_nand for our armadeus apf27 test board) we will have
	# a kernel crash during boot because flashmon is actually loaded before the
	# nand driver and get a segfault trying to obtain the mtd device.
	# Pushing flashmon.o to the end of the list is NOT a long term solution
	# because the way the object file are linked is very platform dependant
	# and may not end on a good order in the initializations ...
	# see http://stackoverflow.com/questions/11642330/linux-built-in-driver-load-order 
	sed -e "/nand_base.o/i\
	obj-\$(CONFIG_MTD_NAND_FLASHMON)		+= flashmon.o\nflashmon-objs := flashmon_core.o flashmon_log.o flashmon_finder.o\n
	" $MAKEFILEOLD > $MAKEFILE
fi

# Sources
rm -f $LINUXDIR/drivers/mtd/nand/flashmon_core.c
rm -f $LINUXDIR/drivers/mtd/nand/flashmon_log.c
rm -f $LINUXDIR/drivers/mtd/nand/flashmon_finder.c
rm -f $LINUXDIR/drivers/mtd/nand/flashmon.h
rm -f $LINUXDIR/drivers/mtd/nand/flashmon_log.h
rm -f $LINUXDIR/drivers/mtd/nand/flashmon_finder.h

$CPY -f $PWD/flashmon_core.c $LINUXDIR/drivers/mtd/nand/flashmon_core.c
$CPY -f $PWD/flashmon_log.c $LINUXDIR/drivers/mtd/nand/flashmon_log.c
$CPY -f $PWD/flashmon_finder.c $LINUXDIR/drivers/mtd/nand/flashmon_finder.c
$CPY -f $PWD/flashmon.h $LINUXDIR/drivers/mtd/nand/flashmon.h
$CPY -f $PWD/flashmon_log.h $LINUXDIR/drivers/mtd/nand/flashmon_log.h
$CPY -f $PWD/flashmon_finder.h $LINUXDIR/drivers/mtd/nand/flashmon_finder.h

echo "Backup for Makefile and Kconfig are $MAKEFILEOLD and $KCONFIGOLD"
