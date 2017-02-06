#!/bin/sh
# Simulates one 256MB NAND flash chip with 2048 bytes pages and 64 
# pages per block. THe chip is divided into two partition of 128MB each.
# Helpful to test flashmon during developement and when adding new 
# features

modprobe mtd
modprobe mtdblock

modprobe nandsim first_id_byte=0x20 second_id_byte=0xaa third_id_byte=0x00 fourth_id_byte=0x15
cat /proc/mtd
