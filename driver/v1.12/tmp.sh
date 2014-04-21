#!/bin/bash 

dir=/home/hwang/sonic/src/driver/working2/
iteration=100000

for i in $(seq 1 1 $iteration)
do 
	echo test > /proc/sonic_tester 
done
