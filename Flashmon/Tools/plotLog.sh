#!/bin/sh
# Take as input a csv file output of /proc/flashmon_log, you can obtain
# such a file with :
# cat /proc/flashmon_log > out.csv
# then :
# ./plotLog.sh out.csv 64
# (for a flash chip monitor with 64 pages per block)
# Why do we need the number of pages per block ? Because the target
# for read and write is a page and the target for erase is a block so
# the index of pages and block are not in the same unit of measurement
# in the output of /proc/flashmon_log.
# With a little trick we make the erase operation address targetting the
# first page of the erased block to have the same unit, then we are able
# to plot erase, read and write addresses on the same graph.
# Another solution could be to change all addresses at a byte level.
# You can limit the x range with an optional third parameter, for 
# example :
#  ./plotLog.sh out.csv 64 12
# will plot only the 12 first seconds of the trace

if [ $# -lt 2 ]; then
	echo "Usage : $0 <output of /proc/flashmon_log> <number of pages per block> [plot title] [max x value]"
	exit
fi

INPUTFILE=$1
PAGESPERBLK=$2
if [ ! "$3" == "" ]; then
	TITLE=$3
else
	TITLE="Untitled plot"
fi
XRANGE=$4
TEMPREAD=/tmp/flashmon_read
TEMPWRITE=/tmp/flashmon_write
TEMPERASE=/tmp/flashmon_erase

# Cleanup
rm -rf $TEMPREAD $TEMPWRITE $TEMPERASE

# Create temp files
cat $INPUTFILE | grep ";R;" > $TEMPREAD
cat $INPUTFILE | grep ";W;" > $TEMPWRITE
cat $INPUTFILE | grep ";E;" | awk -v ppb=$PAGESPERBLK 'BEGIN{FS=";"}{print $1";"$2";"$3*ppb}' > $TEMPERASE

drive_gnuplot(){
	echo "set datafile sep ';'"
	echo "set title '$TITLE'"
	echo "set xlabel 'Time (s)'"
	echo "set ylabel 'Pages hit indexes (block erase shown on the first page)'"
	echo "set grid"
	# echo "set key at 50,28500 box"
	echo "set key top right box"
	
	if [ ! "$XRANGE" == "" ]; then
		echo "set xrange [*:$XRANGE]"
	fi
	
	READPLOT="'$TEMPREAD' u 1:3 ti 'Page read',"
	WRITEPLOT="'$TEMPWRITE' u 1:3 ti 'Page write',"
	ERASEPLOT="'$TEMPERASE' u 1:3 ti 'Block erase (first page)',"
	
	plotline="plot"
	if [ ! "`cat $TEMPREAD`" == "" ]; then
		plotline="$plotline $READPLOT"
	fi
	if [ ! "`cat $TEMPWRITE`" == "" ]; then
		plotline="$plotline $WRITEPLOT"
	fi
	if [ ! "`cat $TEMPERASE`" == "" ]; then
		plotline="$plotline $ERASEPLOT"
	fi
	
	echo "${plotline%?}"
	
}

drive_gnuplot | gnuplot -persist
