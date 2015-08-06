#! /usr/bin/python
import sys, os, argparse, numpy, math, textwrap

verbose=False

def dprint(s):
    if verbose:
	print s

# Approximately compute number of idles required to meet target rate
def compute_idle(plen, rate):
    x = (plen + 8) * 10.0 / rate - plen - 8

    x = (int(x) & ~3) + ((4 - plen %4) & 3)

    dprint('{} {} {}'.format(plen, rate, x))

    return x

def generate_chirp_packets(args):

    f=open(args.output, 'w')

    c=0
    chirp_dist=12
    for s in range (0, args.sample_cnt):
        cur_rate=args.first_rate
        delta = args.sample_freq if chirp_dist <= 12 else chirp_dist + args.idle
        chirp_dist=args.sample_freq

        while(1):
            for i in range (0, args.repeat):
                idle = compute_idle(args.len, cur_rate)
                if idle < 12 :
                    print '{} Gbps: Not possible'.format(cur_rate)
                    break

                # first packet in a single chain
                f.write('{} {} {} {}\n'.format(c, 1, args.len, delta))
                c+=1
                # rest packets
                for i in range(args.pkt_cnt - 1)  :
                    f.write('{} {} {} {}\n'.format(c, 1, args.len, idle))
                    c+=1
             
                if args.mode==2:
                    distance = args.idle - ( args.len * args.pkt_cnt + idle * (args.pkt_cnt - 1)) 
                else:
                    distance = args.idle

                dprint("{} Gbps: Generating {} {}B packets with {} idles, distance from previous chain is {}".format(cur_rate, args.pkt_cnt, args.len, idle, distance))

                delta = idle if distance < idle else distance
            
            chirp_dist = chirp_dist - (args.len * args.pkt_cnt + idle * (args.pkt_cnt - 1) + distance) * args.repeat

            cur_rate += args.delta
            if cur_rate > args.end_rate:
                break

    f.close()
    

def main():
    global verbose

    parser=argparse.ArgumentParser(description='Generating Chirp Packets',
            formatter_class = argparse.RawDescriptionHelpFormatter)
    parser.add_argument('-o', '--output', type=str, default="tmp.info",
            help='Output file, default is tmp.info')
    parser.add_argument('-l', '--len', type=int,
            help='Chirp Packet Length', default='792')
    parser.add_argument('-n', '--pkt_cnt', type=int, default=20,
            help='# of packets in a single chirp chain')
    parser.add_argument('-i', '--idle', type=int,
            help='Distance between the first packets of two chirp chains in # of bytes', default='10000000000')
    parser.add_argument('-f', '--first_rate', type=float, default=0.5,
	    help='The data rate of the first chirp chain')
    parser.add_argument('-e', '--end_rate', type=float, default=9.5,
	    help='The data rate of the last chirp chain')
    parser.add_argument('-d', '--delta', type=float, default=.5,
	    help='The delta rate between ajacent chirp chains')
    parser.add_argument('-m', '--mode', type=int, default=1,
	    help='Mode setting distance between chains 1: Han 2: Kisuh')
    parser.add_argument('-r', '--repeat', type=int, default=1,
        help='Number of repeats for each data rate')
    parser.add_argument('-s', '--sample_cnt', type=int, default=1,
        help='Number of samples to collect.')
    parser.add_argument('-g', '--sample_freq', type=int, default='10000000000',
        help='Distance between first packet of samples, in terms of bytes.')
    parser.add_argument('-v', '--verbose', action='store_true', default=False)

    args=parser.parse_args()

    if args.verbose:
        verbose=True

    if verbose:
        print args

    generate_chirp_packets(args)

if __name__ == "__main__":
    main()
