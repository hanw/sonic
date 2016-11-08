#! /bin/bash

prefix=$1

# source info
mac_src="00:60:dd:45:39:c7"
ip_src=192.168.2.1
port_src=5000

# dstination info
mac_dst="00:60:dd:45:39:5a"
ip_dst=192.168.2.2
port_dst=5000

# packet info
pkt_cnt=100000
idle=13738
pkt_len=1518

SONIC_SERVER="sonic2.cs.cornell.edu"      # FIXME
SONIC_PATH="sonic/driver"

echo 
echo "Demo 1: Profiling"
ssh -i ~/.ssh/id_rsa2 $SONIC_SERVER "$SONIC_PATH/bin/run_sonic.sh -m pkt_gen,pkt_cap -d 5 -w 2 --mac_src $mac_src --ip_src $ip_src --port_src $port_src --mac_dst $mac_dst --ip_dst $ip_dst --port_dst $port_dst -c $pkt_cnt -l $pkt_len -i $idle -p $SONIC_PATH/result/${prefix}_profiling"
scp -i ~/.ssh/id_rsa2 $SONIC_SERVER:$SONIC_PATH/result/${prefix}_profiling_${pkt_len}_${idle}.tmp.ipd result/.
gnuplot -e "input_file=\"result/${prefix}_profiling_${pkt_len}_${idle}.tmp.ipd\"; output_file=\"result/${prefix}_profiling_${pkt_len}_${idle}_pdf.eps\";" profile.gnuplot
open result/${prefix}_profiling_${pkt_len}_${idle}_pdf.eps

