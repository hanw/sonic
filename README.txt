
# compile
$> make

# loading 
$> ./load.sh

# testing rx pcs:
$> echo tester mode=rx_idle,rx_idle duration=5 > /proc/sonic

# testing pktgen, pktcap:
$> sonic/driver/bin/run_sonic.sh
