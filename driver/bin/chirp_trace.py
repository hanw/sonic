#! /usr/bin/python 
import sys, os, argparse, numpy, math, textwrap, random

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

# rate in Gbps
def compute_idles(pkts, rate, mode, begin):
    total_data_bytes = sum(pkts)

    if mode == 1:
        total_bytes = int(total_data_bytes * 10.0 / rate)
        tmp = (total_bytes - total_data_bytes ) / float(len(pkts) - 1)
        tmp = int(tmp)

        if tmp < 12:
            print '{} Gbps: Nos Possible {} {} {}'.format(rate, total_data_bytes, total_bytes, tmp)
            return None

        idles = [ tmp for x in range(len(pkts) - 1)]
        idles.insert(0, begin)

    else :
        tmp = ( (total_data_bytes * 10.0 / rate) - pkts[len(pkts) - 1] ) / float(len(pkts) - 1)
        tmp = int(tmp)

        idles = [begin]

        for x in range(len(pkts) - 1):
            if tmp - pkts[x]  < 12:
                print '{} Gbps: Not Possible {} {} {} {}'.format(rate, total_data_bytes, len(pkts), tmp, pkts[x])
                sys.exit(0)
                return None

            idles.append(tmp - pkts[x])
    
    return idles

#max = 6400000
def read_trace(args):

    total_pkts = int((args.end_rate - args.first_rate) / args.delta + 1) * args.pkt_cnt * args.repeat * args.sample_cnt

    x = random.randint(0, 6400000 - total_pkts * 2)
#    x = 0

    fi=open(args.input, 'r')

    pkts = []
    c = 0
    for line in fi:
        if c == x:
            break
        c += 1

    c = 0
    for line in fi:
        sep=line.split()
        pkts.append(int(sep[2]))

        c += 1
        if c == total_pkts:
            break

    fi.close()

    return pkts;

def generate_chirp_packets(args):

    all_pkts = read_trace(args)

    fo=open(args.output, 'w')

    i=0
    for s in range(args.sample_cnt):
        cur_rate = args.first_rate
        delta = args.sample_freq 

        while(1):
            for r in range(args.repeat):
                pkts = all_pkts[i:i+args.pkt_cnt]
    
                if i == 0:
                    begin = 16
                elif delta != 0 and i % (((args.end_rate - args.first_rate) / args.delta + 1) * args.pkt_cnt) == 0:
                    begin = args.sample_freq
                else:
                    begin = args.gap

                idles= compute_idles(pkts, cur_rate, args.mode, begin)

                if idles == None:
                    print "is this error?", cur_rate
                    break # ???

                for c in range(args.pkt_cnt):
                    fo.write('{} {} {} {}\n'.format(i+c, 2, pkts[c], idles[c]))

                i += args.pkt_cnt
                
            cur_rate += args.delta
            if cur_rate > args.end_rate:
                break

    fo.close()
    

def main():
    global verbose

    parser=argparse.ArgumentParser(description='Generating Chirp Packets from a trace',
            formatter_class = argparse.RawDescriptionHelpFormatter)
    parser.add_argument('-o', '--output', type=str, default="tmp.info",
            help='Output file, default is tmp.info')
    parser.add_argument('-i', '--input', type=str, default="tmp_input.info",
            help='Input file, default is tmp_input.info')
    parser.add_argument('-n', '--pkt_cnt', type=int, default=20,
            help='# of packets in a single chirp chain')
    parser.add_argument('-c', '--chain_cnt', type=int, default=40,
            help='# of chains')
    parser.add_argument('-g', '--gap', type=int,
            help='Distance between two chirp chains in # of bytes', default='10000000000')
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
    parser.add_argument('-q', '--sample_freq', type=int, default='10000000000',
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
