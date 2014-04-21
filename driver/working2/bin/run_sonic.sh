#! /bin/bash
SONIC_DIR=$HOME/sonic/src/driver/v1.11
SONIC_RESULT_LOG_DIR=${SONIC_DIR}/log
date=`date +%Y-%m-%d-%H%M`
RESULT_LOG=$SONIC_RESULT_LOG_DIR/log-$date

num=1
pkt_len=1518
pkt_cnt=1000000
idle=3562
duration=10
mac_wait=5
chain_idle=12
chain_num=100
mode=30
retry=0
process_path=""
replay_path=""
processed=0
nstart=0
delta=128

function check_device {
    rm -f ~/dev/sonic_*
    devnum=`grep sonic /proc/devices| awk '{print $1}'`
    sudo mknod ~/dev/sonic_control c $devnum 0
    sudo mknod ~/dev/sonic_port0 c $devnum 1
    sudo mknod ~/dev/sonic_port1 c $devnum 2
    sudo chown kslee ~/dev/sonic_*
}

function usage {
    echo $0 [-n num of loops] [-l pkt_len] [-c pkt_cnt] [-i idle] [-d duration] [-w wait] [-e chain_idle] [-h chain_num] [-m mode] [-p path]
    exit 1
}

function run_sonic {
    echo tester mode=$mode pkt_len=$pkt_len duration=$duration idle=$idle pkt_cnt=$pkt_cnt chain_idle=$chain_idle wait=$mac_wait chain_num=$chain_num delta=$delta > /proc/sonic_tester 

    error=`dmesg | tail -n 10 | grep "ERROR" | wc -l`
    if [ $error -gt 0 ]; then
        echo Hardware Error!
        exit 1
    fi
    error=`dmesg | tail -n 10 | grep "sonic_dma" | wc -l`
    if [ $error -gt 0 ]; then
        echo DMA Error!
        exit 1
    fi

}

function pre_process {
    replay_path=$1
    $SONIC_DIR/sonic_ins 0 $replay_path
}

function post_process {
    result_path=$1
    $SONIC_DIR/sonic_cap 1 $result_path
    #FIXME
    processed=`sonic_parse.py -i ${result_path} -d -s 0060dd45390d -t 0060dd45395a | grep "Processed" | awk '{print $2}'`
}

while getopts "n:l:c:i:d:w:e:h:m:p:y:rs:t:" OPTION
do
    case $OPTION in
    n) num=$OPTARG
    ;;
    l) pkt_len=$OPTARG
    ;;
    c) pkt_cnt=$OPTARG
    ;;
    i) idle=$OPTARG
    ;;
    d) duration=$OPTARG
    ;;
    w) mac_wait=$OPTARG
    ;;
    e) chain_idle=$OPTARG
    ;;
    h) chain_num=$OPTARG
    ;;
    m) mode=$OPTARG
    ;;
    p) process_path=$OPTARG
    ;;
    y) replay_path=$OPTARG
    ;;
    r) retry=1
    ;;
    s) nstart=$OPTARG
    ;;
    t) delta=$OPTARG
    ;;
    \?) usage;;
    esac
done

/home/kslee/bin/enable2ports.sh 2>&1 > /dev/null
/home/kslee/bin/enable5gts.sh 2>&1 > /dev/null

check_device

for ((k=nstart;k<nstart+num;k++));
do
    nret=0
    while :
    do
        echo "Running SoNIC" mode=$mode pkt_len=$pkt_len duration=$duration idle=$idle pkt_cnt=$pkt_cnt chain_idle=$chain_idle wait=$mac_wait chain_num=$chain_num delta=$delta

        if [[ $replay_path != "" ]]; then
            pre_process $replay_path
        fi

        run_sonic 

        if [[ $process_path != "" ]] ; then
            result_path=${process_path}_${pkt_len}_${idle}_${k}.tmp
            echo "Processing and Parsing"
            post_process $result_path
        fi

        if [[ $retry == 1 ]]; then
            if [ $processed -ne $pkt_cnt ]; then
                echo "Retry" $processed "<" $pkt_cnt
                nret=$((nret+1))
                if [ $nret -eq "8" ]; then
                    echo "Too many retries"
                    exit 1
                fi
                sleep 20
                continue
            else
                break;
            fi
        else
            break;
        fi
    done
    echo "$processed"
done

exit 0
