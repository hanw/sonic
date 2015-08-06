#! /bin/bash
SONIC_BIN=$HOME/sonic/bin
TESTER=$HOME/sonic/src/driver/working2/kernel/tester

TMP=regression.tmp

function Result {
    if [[ $1 ==  0 ]]; then
        echo $2 Successed
    else
        echo $2 Failed
    fi
}

# test for 1518 and 64~72 B packets
function Test {
    mode=$1
    extra=$2
    togrep=$3
    name=$4
#    echo $mode $extra $togrep $name
#    for len in 1518 64;
    for len in 1518 72 71 70 69 68 67 66 65 64;
    do
        $TESTER mode=$mode pkt_len=$len pkt_cnt=30000000 $extra > $TMP

        thru=`grep $togrep $TMP | awk '{print $5}'`
        target=`$SONIC_BIN/ref_throughput.py -l $len -i 12 | awk '{print $2}'`
        res=`echo "$thru < $target" | bc`
	echo $len $thru $target
        Result $res "$name $len ($thru/$target) "
    done
    echo ""
}

cd ../kernel
make clean 2>&1 > /dev/null
make tester SONIC_DDEBUG=0 SONIC_DEBUG=0 2>&1 > /dev/null
Result $?  "Compile"
cd ../bin

# CRC comparison
$TESTER mode=0 2>&1 > /dev/null
Result $?  "CRC Comparison"

# FastCRC Performance 
Test 1 "gen_mode=3" "tx_mac0" "FAST CRC"

#encoder
Test 2 "" "tx_pcs0" "ENCODER"

#Gen
Test 10 "" "tx_pcs0" "PKT_GEN"

#Rpt
#Test 13 "" "tx_pcs0" "PKT_RPT"

#decoder
Test 3 "" "rx_pcs0" "DECODER"

#Recv
Test 20 "" "rx_pcs0" "PKT_RCV"

#Cap
Test 23 "" "rx_pcs0" "PKT_CAP"

rm -f $TMP
