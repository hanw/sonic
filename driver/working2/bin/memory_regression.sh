#! /bin/bash
SONIC_BIN=$HOME/sonic/bin

# test for 1518 and 64~72 B packets
function Test {
    mode=$1
    name=$2
#    echo $mode $extra $togrep $name
    echo $2
    for len in 1518;
#    for len in 1518 72 71 70 69 68 67 66 65 64;
    do
        valgrind ./tester mode=$mode pkt_len=$len pkt_cnt=30000000 $extra 
    done
    echo ""
}

make clean 2>&1 > /dev/null
make tester SONIC_DDEBUG=0 SONIC_DEBUG=0 SONIC_FAST_CRC=0 2>&1 > /dev/null

Test 0 "..."
# FastCRC Performance 
#Test 1 "FAST CRC"

#encoder
#Test 2 "ENCODER"

#decoder
#Test 3 "DECODER"

#Gen
#Test 10 "PKT_GEN"

#Recv
#Test 20 "PKT_RCV"

#Cap
#Test 23 "PKT_CAP"
