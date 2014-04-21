#! /bin/bash
duration=10

if [ $# -le 0 ]; then
	echo "Usage $0 <mode>"
	exit 1
fi

case $1 in
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

echo tester mode=$mode duration=$duration > /proc/sonic_tester


