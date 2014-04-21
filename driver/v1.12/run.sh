#! /bin/bash
begin=64
end=64
step=1
num=1
duration=100
idle=12
pkt_cnt=1000000
chain_idle=6372
chain_num=100000
mac_wait=5
mqueue=1
delta=128

if [ $# -le 0 ]; then
	echo "Usage $0 <mode>"
	exit 1
fi

case $1 in
crc) mode=1;;
encode) mode=10;;
gen) mode=11;;
gen_covert) mode=14;;

decode) mode=20;;
rec) mode=21;;
cap) mode=23;;

fwrd) mode=30;;
forward2) mode=31;;
forward) mode=32;;

pkt) mode=40;;
pkt2) mode=41;;
pkt_full) mode=39;;
pkt_cap) mode=42;;
pkt_cap2) mode=43;;
pkt_gencap) mode=44;;
covert_cap) mode=45;;
covert_cap2) mode=46;;
pkt_rep)    mode=47;;

custom) mode=48;;

idle_rx) mode=60;;
idle_rx2) mode=61;;
idle_tx) mode=62;;
idle_tx2) mode=63;;
idle) mode=64;;
idle2) mode=65;;
idle_txtx) mode=66;;
idle_rxrx) mode=67;;
idle_full) mode=68;;

dma_rx) mode=50;;
dma_rx2) mode=51;;
dma_tx) mode=52;;
dma_tx2) mode=53;;
dma_rxtx) mode=54;;
dma_rxtx2) mode=55;;
dma_txtx) mode=56;;
dma_rxrx) mode=57;;
dma_full) mode=58;;

pcs2) mode=37;;

*) echo "Wrong mode"; exit 1;;
esac

/home/kslee/bin/enable2ports.sh
/home/kslee/bin/enable5gts.sh

echo tester mode=$mode begin=$begin end=$begin duration=$duration idle=$idle pkt_cnt=$pkt_cnt chain_idle=$chain_idle wait=$mac_wait chain_num=$chain_num > /proc/sonic_tester


