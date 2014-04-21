#! /bin/bash
sonic_path="/home/kslee/sonic/src/driver/working2/"
log_path="$sonic_path/res/multi_chain_10/6500"

nloops=100
mode=47
duration=60
mac_wait=35

/home/kslee/bin/enable2ports.sh
/home/kslee/bin/enable5gts.sh

idle_64=(1572)
#idle_64=(1572 6372)
idle_1518=(1596 35508 137236)

for (( k=84 ;k < $nloops ; k ++))
do
#    for l in 1518 64 
	for l in 64 
    do
        if [ $l -eq "64" ]; then
            idles=${idle_64[@]}
            num=1000000
        else
            idles=${idle_1518[@]}
            num=1000000
        fi

        for idle in $idles
        do

            j=$((k+1))
            in_name="${log_path}/${l}_${idle}_${k}.tmp.info"
            log_name="${log_path}/${l}_${idle}_${j}.tmp"

            echo $l $num $idle $k

            while : 
            do
                echo "Reading $in_name"
                ${sonic_path}/sonic_ins 0 ${in_name}
                sleep 1

                echo tester mode=$mode begin=$l end=$l step=1 nloops=1 duration=$duration idle=$idle pkt_cnt=$num wait=$mac_wait > /proc/sonic_tester 

                error=`dmesg | tail -n 5 | grep "Waited" | wc -l`
                if [ $error -gt 0 ]; then
                    echo Error!
                    exit 1
                fi


                cnt=`dmesg | tail -n 10 | grep "ins_loop" | awk '{if ($3=="SONIC:") print $12; else print $11;}'`

#                echo "Received $cnt"

                ${sonic_path}/sonic_cap 1 ${log_name}

                lcnt=`wc ${log_name} | awk '{print $1}'`

                echo $cnt $lcnt

                processed=`sonic_parse.py -i ${log_name} -d | grep "Processed" | awk '{print $2}'`

                if [ $processed -ne $num ]; then
                    echo "Retry $processed"
                    sleep 1
                    continue
                else
                    echo "done $processed"
                    sleep 1
                    break;
                fi
            done

        done
    done
done
