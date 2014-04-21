#! /bin/bash

COMMON_PATH=`pwd`
RESULT_PATH=${COMMON_PATH}/result/user/log

if [ ! -d $RESULT_PATH ]
then
	mkdir $RESULT_PATH
fi

begin=1518
end=64
step=1
nloops=8
duration=1

# modes
CRC=1
LOG=2
ENCODE=10
GEN=11
DECODE=20
REC=21

PCS=30
PCS2=31
PCS_CROSS=32
PKT=38
PKT2=39
PKT_CROSS=40

MAC_CAP_FWRD=36

# CRC modes
FASTCRC=0
LOOKUP=1
BITCRC=2
FASTCRC2=3

# scrambler modes
OPT=0
BITWISE=1

echo "Testing script to evaluate SoNIC Userspace"
echo "Results will be stored here: " $RESULT_PATH

for tests in log
do
	crcmode=0
	scramblermode=0
	pcs_level=1
	case $tests in
	fastcrc) mode=$CRC
		crcmode=0
		;;
	lookup) mode=$CRC
		crcmode=1
		;;
	bitcrc)	mode=$CRC
		crcmode=2
		;;
	fastcrc2)
		mode=$CRC
		crcmode=3
		;;	
	encode) mode=$ENCODE
		pcs_level=0
		;;
	encode2) mode=$ENCODE
		;;
	encode_bit) mode=$ENCODE
		scramblermode=1
		;;
	gen)	mode=$GEN
		;;
	decode) mode=$DECODE
		pcs_level=0
		;;
	decode2) mode=$DECODE
		;;
	decode_bit) mode=$DECODE
		scramblermode=1
		;;
	rec) mode=$REC;;
	pcs) mode=$PCS;;
	pcs2) mode=$PCS2;;
	pcs_cross) mode=$PCS_CROSS;;
	pkt) mode=$PKT;;
	pkt2) mode=$PKT2;;
	pkt_cross) mode=$PKT_CROSS;;
	mac_cap_fwrd) mode=$MAC_CAP_FWRD;;
	*) echo "Wrong mode"; exit 1;;
	esac
	
	echo "Mode = $mode"
	echo $tests
	makeflag="SONIC_CRC=${crcmode} SONIC_SCRAMBLER=${scramblermode} SONIC_PCS_LEVEL=${pcs_level}"
#	echo $makeflag
	make clean > /dev/null 2>&1
	make ${makeflag} > /dev/null 2>&1

	date=`date +%Y-%m-%d-%H%M`
	filename="user_${tests}_begin-${begin}_end-${end}_step-${step}_nloops-${nloops}_duration-${duration}_${date}"

	echo $filename

	$COMMON_PATH/tester mode=${mode} begin=${begin} end=${end} step=${step} nloops=${nloops} duration=${duration} > ${RESULT_PATH}/${filename}

done
