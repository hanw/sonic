#! /bin/bash

sonic1="sonic1.systems.cs.cornell.edu"
bin_path="/home/kslee/bin/sonic_altera_comb/csr_scripts/"
log_path="/home/kslee/sonic/src/driver/working/result/"
date=`date +%Y-%m-%d-%H%M`

#init, clear
function control_mac {
	ssh $sonic1 "cd $bin_path;./mac_automator.sh $1"
}

function capture_mac {
	ssh $sonic1 "cd $bin_path;./mac_automator.sh capture"
}

begin=149
end=64
step=1
port=0
nloops=4

log_name="${log_path}mac_capture_begin-${begin}_end-${end}_nloops-${nloops}-${date}"

if [ $# -le 0 ]; then
	echo "Usage $0 <mode>"
	exit 1
fi

case $1 in
gen) mode=11;;
rec) mode=21;;
*) echo "Wrong mode"; exit 1;;
esac

$HOME/bin/enable2ports.sh
$HOME/bin/enable5gts.sh

#make clean
#make
#make load

echo "SONIC: [sonic_print_args] begin = $begin" >> $log_name
echo "SONIC: [sonic_print_args] end = $end" >> $log_name
echo "SONIC: [sonic_print_args] nloops = $nloops" >> $log_name

#mac init
control_mac init > /dev/null 2>&1

#mac clear
control_mac clear > /dev/null 2>&1

for (( j=$begin ; j>=$end ; j-- ))
do
	echo "testing $j"
	echo "TESTING $j" >> $log_name

	for (( k=0 ; k <$nloops; k++ ))
	do 

		#run sonic
		echo tester mode=$mode begin=$j end=$j step=$step nloops=1 > /proc/sonic_tester 
#		sleep 1

		error=`dmesg | tail -n 10 | grep "ERROR" | wc -l`
		if [ $error -gt 0 ]; then
			echo Error!
			exit 1
		fi

		dmesg | tail -n 13 >> $log_name

		#mac capture
		capture_mac |tail -n 36 >> $log_name
	done

	sleep 1

done

sudo rmmod sonic_tester
