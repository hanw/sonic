#! /bin/bash
sonic_path="/home/kslee/sonic/src/driver/working2/"
log_path="$sonic_path/res/multi/6500"

nloops=2
mode=42
duration=60
mac_wait=35

/home/kslee/bin/enable2ports.sh
/home/kslee/bin/enable5gts.sh

idle_64=(8 48 168 648)
idle_1518=(170 1018 3562 13738)

for (( k=1 ;k < $nloops ; k ++))
do
#	for l in 64 
	for l in 1518 64 
	do
		for i in  3   
#		for i in  0 2 3
		do 
		if [ $l -eq "64" ]; then
			num=15000000
			idle=${idle_64[$i]}
		else
			num=1000000
			idle=${idle_1518[$i]}
		fi

			log_name="${log_path}/${l}_${idle}_${k}.tmp"
			#log_name="${log_path}${l}_${idle}_base.txt"

			echo $l $num $idle $k

			echo tester mode=$mode begin=$l end=$l step=1 nloops=1 duration=$duration idle=$idle pkt_cnt=$num wait=$mac_wait > /proc/sonic_tester 

			error=`dmesg | tail -n 5 | grep "Waited" | wc -l`
			if [ $error -gt 0 ]; then
				echo Error!
				exit 1
			fi

			echo "done"

			cnt=`dmesg | tail -n 10 | grep "log_loop" | awk '{if ($3=="SONIC:") print $12; else print $11;}'`

			echo "Received $cnt"

			${sonic_path}/sonic_cap	1 ${log_name}

			lcnt=`wc ${log_name} | awk '{print $1}'`

			echo $cnt $lcnt

		done
	done
done
