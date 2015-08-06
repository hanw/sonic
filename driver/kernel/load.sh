#! /bin/bash

make

if [ `lsmod | grep -o sonic` ]; then
    sudo rmmod sonic
fi

sudo insmod ./sonic.ko || exit 1

sudo rm -f /dev/sonic_*

devnum=`grep sonic /proc/devices| awk '{print $1}'`
sudo mknod /dev/sonic_control c $devnum 0
sudo mknod /dev/sonic_port0 c $devnum 1
sudo mknod /dev/sonic_port1 c $devnum 2
sudo chmod 666 /dev/sonic_*

../bin/enable2ports.sh
../bin/enable5gts.sh

cd ../examples
make
