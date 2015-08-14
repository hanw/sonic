#! /bin/bash
SONIC_DIR=$HOME/sonic/git/driver
date=`date +%Y-%m-%d-%H%M`

pkt_len=1518
pkt_cnt=100000
idle=13738
duration=10
mac_wait=5
mode=pkt_gen,pkt_cap

mac_src="00:60:dd:45:39:0d"
mac_dst="00:60:dd:45:39:5a"
ip_src="192.168.4.12"
ip_dst="192.168.4.13"
port_src=5000
port_dst=5008

vlan_id=2200
gen_mode=0

retry=0
process_path=""
replay_path=""
processed=0


function check_device {
    rm -f ~/dev/sonic_*
    mkdir -p ~/dev
    devnum=`grep sonic /proc/devices| awk '{print $1}'`
    sudo mknod ~/dev/sonic_control c $devnum 0
    sudo mknod ~/dev/sonic_port0 c $devnum 1
    sudo mknod ~/dev/sonic_port1 c $devnum 2
    sudo chown kslee ~/dev/sonic_*
}

function usage {
    echo $0 [-l pkt_len] [-c pkt_cnt] [-i idle] [-d duration] [-w wait] [-m mode] [-p path] [-y path] [--mac_src] [--mac_dst] [--ip_src] [--ip_dst] [--port_src] [--port_dst]
    exit 1
}

function run_sonic {
    echo tester mode=$mode pkt_len=$pkt_len duration=$duration idle=$idle pkt_cnt=$pkt_cnt wait=$mac_wait mac_src=$mac_src mac_dst=$mac_dst ip_src=$ip_src ip_dst=$ip_dst port_src=$port_src port_dst=$port_dst vlan_id=$vlan_id gen_mode=$gen_mode delta=$epsilon > /proc/sonic_tester 

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
    $SONIC_DIR/example/sonic_ins 0 $replay_path
}

function post_process {
    result_path=$1
    $SONIC_DIR/example/sonic_cap 1 $result_path
    #FIXME

    echo "Processing captured packets"
    processed=`$HOME/sonic/bin/sonic_parse.py -i ${result_path} -d -s $mac_src -t $mac_dst | grep "Processed" | awk '{print $2}'`
}

while getopts "l:c:i:d:w:m:p:r:g:v:e:-:" OPTION
do
    case $OPTION in
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
    m) mode=$OPTARG
    ;;
    p) process_path=$OPTARG
    ;;
    r) replay_path=$OPTARG
    ;;
    v) vlan_id=$OPTARG
    ;;
    g) gen_mode=$OPTARG
    ;;
    e) epsilon=$OPTARG
    ;;
    -)
        case "${OPTARG}" in
            ip_src) ip_src=${!OPTIND}
            ;;
            ip_dst) ip_dst=${!OPTIND}
            ;;
            mac_src) mac_src=${!OPTIND}
            ;;
            mac_dst) mac_dst=${!OPTIND}
            ;;
            port_src) port_src=${!OPTIND}
            ;;
            port_dst) port_dst=${!OPTIND}
            ;;
        esac
        OPTIND=$((OPTIND + 1))
        ;;
    \?) usage;;
    esac
done

echo "Running $mode for $duration seconds"
if [[ $mode == *,* ]]
then
    idx=`expr index $mode "," `
    mode1=${mode:0:idx-1}
    mode2=${mode:idx}
else
    mode1=$mode
    mode2=""
fi

if [ $mode1 == "pkt_gen" ]; then
    echo "Port 0 is generating $pkt_cnt $pkt_len byte UDP packets with $idle /I/s between them"
    echo "Packets will be generated after $mac_wait seconds and have following header fields: "
    echo "   MAC SRC = $mac_src, MAC DST= $mac_dst",
    echo "   IP SRC = $ip_src, IP DST = $ip_dst",
    echo "   PORT SRC = $port_src, PORT DST = $port_dst"
    echo "   vlan ID= $vlan_id (if enabled)"
    if [ $gen_mode == "3" ]; then
        echo "   Creating a covert channel modulating $epsilon /I/s"
    fi
fi

echo

if [ $mode2 == "pkt_cap" ]; then
    echo "Port 1 is capturing packets"
fi

echo 

/home/kslee/bin/enable2ports.sh 2>&1 > /dev/null
/home/kslee/bin/enable5gts.sh 2>&1 > /dev/null

check_device

    nret=0
    while :
    do

        if [[ $replay_path != "" ]]; then
            pre_process $replay_path
        fi

        run_sonic 

        if [[ $process_path != "" ]] ; then
            result_path=${process_path}_${pkt_len}_${idle}.tmp
            echo "Processing and Parsing"
            post_process $result_path
        fi

        if [[ $retry == 1 ]]; then
            if [ $processed -ne $pkt_cnt ]; then
                echo "Retry" $processed "<" $pkt_cnt
                nret=$((nret+1))
                if [ $nret -eq "4" ]; then
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

exit 0
