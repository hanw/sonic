#! /usr/bin/python
import sys, os, argparse, numpy, math, textwrap, random
import sonic

verbose=False

def dprint(s):
    if verbose:
        print s

def read_binary_file(f, pkt_cnt):
    bytes_read = open(f, 'rb').read(pkt_cnt / 8 +8)

    bits=[]
    i=0
    while True:
        for b in bytes_read:
            for j in range(8):
                bits.append((ord(b) >> j) & 0x1)
                i += 1

                if i == pkt_cnt:
                    return bits

def generate_sequence(args):
    total_bits = args.pkt_cnt * args.multibit
    if args.input is not None:
        bits=read_binary_file(args.input, total_bits)
        return bits

    if args.message is not None:
        bs=sonic.msg_to_bits(args.message)

        bits=[]
        i=0
        while True:
            for b in bs:
                bits.append(b)
                i+= 1

                if i == total_bits:
                    return bits 

    bits=[]
    for s in range (0, total_bits):
        bits.append(random.randint(0,1))

    return bits

def bits_stats(bits, multibit):
    counters=[0 for i in range(pow(2,multibit))]

    if multibit is not 0:
        for i in range(len(bits) / multibit):
            bs = bits[i:i + multibit]

            value = 0 
            for i in range(len(bs)):
                value = value * 2 + bs[i]

            counters[value] += 1
    else:
        counters[0]=len(bits)

    dprint (counters)

def compute_gap(args, bits, idx):

    if args.multibit is 0:
        return (12 if args.gap < 12 else args.gap), 0, 0

    offset = idx * args.multibit

    bs = bits[idx:idx+args.multibit]

    value = 0 
    for i in range(len(bs)):
        value = value * 2 + bs[i]


    m = pow(2, len(bs)-1)

    new_gap = int(args.gap - (m - .5) * args.epsilon + value * args.epsilon)
#    if value < m:
#        epsilon = (value - m) * args.epsilon
#    else:
#        epsilon = (value - m + 1) * args.epsilon

#    new_gap = int(args.gap + epsilon)
#    if new_gap < 12:
#        new_gap = 12

    return new_gap, value

def generate_pkts(args):
    if args.stream is None:
        pkts= [args.len for x in range(args.pkt_cnt)]
        return pkts

    f=open(args.stream, 'r')
    
    pkts=[]
    i=0
    c=0
    for line in f:
        c += 1
        if c < 50000:
            continue

        x=line.split()
        pkts.append(x[2])
        i += 1;
        if i == args.pkt_cnt + 1:
            break

    f.close()

    return pkts

def generate_covert_packets(args):

    bits=generate_sequence(args)

    bits_stats(bits, args.multibit)

    pkts=generate_pkts(args)

    f=open(args.output, 'w')

    f.write('0 2 {} {}\n'.format(pkts[0], 16))

    for i in range(args.pkt_cnt):
        gap, value = compute_gap(args, bits, i)
        new_id = (i+1) | value << 24

        f.write('{} {} {} {}\n'.format(new_id, 2, pkts[i+1], gap))
        dprint ('{} /{} {} {} {} ({}) {}'.format(hex(i+1), args.pkt_cnt, hex(value), hex(new_id), gap, epsilon, pkts[i+1]))
    
    f.close()
    

def main():
    global verbose

    parser=argparse.ArgumentParser(description='Generating multi-bit covert channel  packets',
            formatter_class = argparse.RawDescriptionHelpFormatter)
    parser.add_argument('-i', '--input', type=str, default=None,
            help='Input file')
    parser.add_argument('-o', '--output', type=str, default="tmp.info",
            help='Output file, default is tmp.info')
    parser.add_argument('-l', '--len', type=int,
            help='Overt Packet Length', default='1518')
    parser.add_argument('-n', '--pkt_cnt', type=int, default=100000,
            help='# of packets')
    parser.add_argument('-g', '--gap', type=int,
            help='Gap in # of idles', default='13738')
    parser.add_argument('-e', '--epsilon', type=float, default=256,
            help='The epsilon to create chupja')
    parser.add_argument('-m', '--message', type=str, default=None, 
            help='Hidden message')
    parser.add_argument('-t', '--multibit', type=int, default=1,
            help='How many bits per gap')
    parser.add_argument('-s', '--stream', type=str, default=None,
            help='pkt size file')
    parser.add_argument('-v', '--verbose', action='store_true', default=False)

    args=parser.parse_args()

    if args.verbose:
        verbose=True

    if verbose:
        print args

    generate_covert_packets(args)

if __name__ == "__main__":
    main()
