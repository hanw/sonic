#! /bin/bash
pkt_len=1518
mode_overt=42
mode_covert=45
pkt_cnt=10001
duration=37
mwait=35

overt=0
covert=1

begin=0
end=1

delta_begin=0
delta_end=9

log=ibm		# TODO TODO TODO 

#overt_log=overt4/${log}
covert_log=covert5/${log}

/home/kslee/bin/enable2ports.sh
/home/kslee/bin/enable5gts.sh

#ssh sonic5 "mkdir -p netslice/timestamping/${overt_log}"
ssh sonic5 "mkdir -p netslice/timestamping/${covert_log}"
#mkdir -p res/${overt_log}
mkdir -p res/${covert_log}

function run_test {
	log_path=$1
	l_idle=$2
	l_delta=$3
	l_mode=$4

	echo idle=$l_idle delta=$l_delta mode=$l_mode 

	while :
	do
		ssh sonic5 "sudo killall -q receiver; netslice/timestamping/receiver eth13 5008 $pkt_cnt > netslice/timestamping/${log_path} &"
		echo tester mode=$l_mode begin=$pkt_len end=$pkt_len duration=$duration idle=$l_idle pkt_cnt=$pkt_cnt wait=$mwait delta=$l_delta > /proc/sonic_tester 

		error=`dmesg | tail -n 5 | grep "Waited" | wc -l`
		if [ $error -gt 0 ]; then
			echo Error!
			exit 1
		fi

		res=`ssh sonic5 "wc -l netslice/timestamping/${log_path}"`
		res_l=`echo $res | colex 1`

		if [ $res_l -ne 10000 ]; then
			echo "Retry $res_l"
			sleep 1
			continue
		else
			dmesg | tail -n 90 >> res/timing_log.txt

			./sonic_cap 1 res/${log_path}.tmp
			sleep 1
			break;
		fi
	done
}

#for idle in 13738 3562 1018 170 12
for idle in 13738 3562 1018 170 
#for idle in 1018 170 
do
	delta=0
	for ((k=$delta_begin;k<$delta_end;k++))
	do

		if [[ $k == 0 ]]; then
			delta=0
		elif [[ $k == 1 ]]; then
			delta=16
		else 
			delta=$((delta*2))
		fi

#		if [[ $idle == 12 ]]; then
#			delta=$((32 * k))
#			idle_base=$((idle + 32 * k))
#		else
#			delta=$((64 * k))
#			idle_base=$((idle + 64 * k))
#		fi

		#echo $delta
		
		for ((j=$begin ; j<$end ; j++))
		do
			fname=${pkt_len}_${idle}_${delta}_${pkt_cnt}_${j}
		
			if [[ $overt == 1 ]] ; then
				echo Overt ${overt_log}/$fname
				echo Overt ${overt_log}/$fname $idle $delta $mode_overt >> res/timinglog.txt

				run_test ${overt_log}/${fname} $idle_base $delta $mode_overt

			fi

			if [[ $covert == 1 ]]; then
				echo Covert ${covert_log}/$fname
				echo Covert ${covert_log}/$fname $idle $delta $mode_covert>> res/timinglog5.txt

				run_test ${covert_log}/${fname} $idle $delta $mode_covert
			fi
		done
	done
done

