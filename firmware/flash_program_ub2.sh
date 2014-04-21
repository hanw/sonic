#!/bin/bash

# FIXME: fix path to your local installation
QUARTUS_ROOTDIR=~/altera/13.0sp1/quartus
NIOS2EDS_ROOTDIR=~/altera/13.0sp1/nios2eds

cable=1

QUARTUS_PGM_PATH=$QUARTUS_ROOTDIR/bin/quartus_pgm

echo ""
echo "Loading $SOF_FILE using cable $cable"
echo ""

# program pfl
$QUARTUS_PGM_PATH -m jtag -c $cable -o "p;S5_PFL.sof"

# convert to .flash
$NIOS2EDS_ROOTDIR/nios2_command_shell.sh sof2flash --input=PCIe_Fundamental.sof --output=flash_hw.flash --offset=0x20C0000 --pfl --optionbit=0x00030000 --programmingmode=PS

# programming with .flash
$NIOS2EDS_ROOTDIR/nios2_command_shell.sh nios2-flash-programmer --base=0x0 flash_hw.flash
$NIOS2EDS_ROOTDIR/nios2_command_shell.sh nios2-flash-programmer --base=0x0 S5_OptionBits.flash
