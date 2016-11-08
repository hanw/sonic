
# compile
$> make

# loading 
$> ./load.sh

# testing rx pcs:
$> echo tester mode=rx_idle,rx_idle duration=5 > /proc/sonic

# testing pktgen, pktcap:
$> sonic/driver/bin/run_sonic.sh

# for demo
clone sonic git repo in your local machine
create a directory
$> mkdir sonic/demo/result

#create a directoy in sonic machine
$> mkdir sonic/driver/result

run demo script on your local machine
#for profiling:
$> ./demo1.sh

# for covert channel:
$> ./demo2.sh
