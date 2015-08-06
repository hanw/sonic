#! /bin/bash

src_ip=192.168.3.30
dst_ip=192.168.3.31
src_mac=00:07:43:11:d7:90
dst_mac=00:07:43:11:f7:90
vlan=1676

# actual gen
./run_sonic.sh --ip_src $src_ip --ip_dst $dst_ip --mac_src $src_mac --mac_dst $dst_mac --port_src 5000 --port_dst 5000 -m pkt_gencap -c 100 -v $vlan -d 10 -w 9

./run_sonic.sh --ip_src $src_ip --ip_dst $dst_ip --mac_src $src_mac --mac_dst $dst_mac --port_src 5000 --port_dst 5000 -m pkt_gencap -c 100000 -v $vlan -d 15 -w 2 -i 30500 -l 1518

