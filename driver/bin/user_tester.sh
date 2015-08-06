#! /bin/bash
begin=64
end=64
step=1
num=1
duration=60
idle=12
pkt_cnt=1000000
chain_idle=30000
mac_wait=35
mqueue=1
delta=128
verbose=1

if [ $# -le 0 ]; then
    echo "Usage $0 <mode>"
    exit 1
fi

SONIC_BIN=$PWD/tester

mode_str=${1^^}
mode=`cat mode.h | awk -v mode=$mode_str '{if ($3 == "MODE_"mode",") print substr($2, 0, length($2)-1)}'`

echo Testing $mode_str $mode
$SONIC_BIN mode=$mode begin=$begin end=$end step=$step nloops=$num verbose=$verbose pkt_cnt=$pkt_cnt idle=$idle

