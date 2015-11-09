#! /usr/bin/python
import sys, os, getopt, numpy, math, argparse, collections
import sonic

def write_binary_file(fname, decoded):
    f = open (fname, "wb")

    for x in decoded:
        f.write(x)

    f.close()

def main():
    parser=argparse.ArgumentParser(description='Analyze captured data')
    parser.add_argument('-i', '--input', type=str,
            help='Input file', required=True)
    parser.add_argument('-o', '--output', type=str,
            help='Output file', required=False)
    parser.add_argument('-m', '--message', type=str,
            help='covert message file')
    parser.add_argument('-d', '--delta', type=int,
            help='Delta', required=True)
    parser.add_argument('-n', '--pkt_cnt', type=int, help='pkt cnt', default=-1 )
    parser.add_argument('-e', '--endhost', action='store_true', default=False,
            help='Endhost timestamping?')
    parser.add_argument('-v', '--verbose', action='store_true', default=False)

    args=parser.parse_args()

    if args.verbose:
        print args

    covert_bits = read_binary_file(args.message)

    one, zero, decoded=sonic.separate_ipds(args.input, args.pkt_cnt, covert_bits, args.delta)

    sonic.dump_dist_bits(args.input+".one.ipd", one)
    sonic.dump_dist_bits(args.input+ ".zero.ipd", zero)

    if args.output:
        dump_covert_decoded(args.output, decoded)
    

if __name__ == "__main__":
    main()
