#!/bin/bash
TESTER_BIN=$PWD/../kernel/tester
SONIC_BIN=$HOME/sonic/bin

rm -f regression.log

TMP=regression.tmp

function passfail {
    len=$1
    thru=$2
    target=$3
    msg=$4

    compare_result=`echo "$2 > $3" | bc`
    if [ $compare_result ];then
        echo "${len}B packet $msg PASSED ($2/$3)"
    else
        echo "${len}B packet $msg FAILED ($2/$3)"
    fi
}

make clean;make tester SONIC_DDEBUG=1


for _mode_str in crc encode decode pkt_gen pkt_rec; do
#for _mode_str in pkt_gen ; do
    mode_str=${_mode_str^^}
    mode=`cat mode.h | awk -v mode=$mode_str '{if ($3 == "MODE_"mode",") print substr($2, 0, length($2)-1)}'`
    echo "Testing $mode_str ($mode)"
#    for len in 64 65 66 67 68 69 70 71 72 1518; do
    for len in 64 1518; do
        $TESTER_BIN mode=$mode begin=$len end=$len step=1 nloops=1 pkt_cnt=20000000 idle=12 > $TMP
        
        # process
        case $_mode_str in
        crc) 
            thru=`grep "mac_tester" $TMP | awk '{print $4}'`
            target=`$SONIC_BIN/ref_throughput.py -l $len -i 12 | awk '{print $2}'`
            passfail $len $thru $target "CRC"
            ;;
        encode) ;&
        decode)
            thru=`grep "pcs_tester" $TMP | awk '{print $4}'`
            target=10.3125
            passfail $len $thru $target "PCS"
            ;;
        pkt_gen) ;&
        pkt_rec)
            thru=`grep "sonic_pkt_" $TMP | awk '{print $4}'`
            target=`$SONIC_BIN/ref_throughput.py -l $len -i 12 | awk '{print $2}'`
            passfail $len $thru $target "MAC"
            thru=`grep "_pcs_" $TMP | awk '{print $4}'`
            target=10.3125
            passfail $len $thru $target "PCS"
            ;;
        *) echo "Wrong mode";;
        esac

    done
done

rm -f $TMP
