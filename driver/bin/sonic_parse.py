#! /usr/bin/python
import sys, os, argparse, numpy, math, textwrap
from sonic import *

verbose=True

def parse_captured_file(ifname, ofname, src, dst, id_offset, pkt_cnt, dump):
    mac1=dst[0:8]
    mac2=dst[8:12] + src[0:4]
    mac3=src[4:12]

    if ofname is None:
        ofname = ifname+'.info'

    print 'Input file is {0}, output file is {1}'.format(ifname, ofname)

    input_file = open(ifname, 'r')
    output_file = open(ofname, 'w')

    is_first_processed = False
    ipgs_bit={}
    ipds_bit={}
    i=0

    plen = 0
    idle = 0
    idlebit = 0
    pipg_byte = 0
    pipg_bit = 0

    prev_plen_byte = 0
    prev_plen_bit = 0
    prev_pid = -1
    lost = 0
    lost_pids=[]

    output_file.write('#{:12s}\t{:5s}\t{:4s}\t{:8s}\t{:12s}\t{:8s}\t{:12s}\n'.\
            format("Packet ID", "Type", "Len", "IPG(B)", "IPG(b)", "IPD(B)", "IPD(b)"))

    for line in input_file:
        sep=line.split()

        ptype = int(sep[0])
        plen = int(sep[1])
        plen_bit = convert_pkt_len_to_bits(plen, ptype)

        ipg_byte = int(sep[2])
        ipg_bit = int(sep[3])

        # filter unmatched packets while aggregating idles
        if sep[4] != mac1 or sep[5] != mac2 or sep[6] != mac3 or sep[7] != "08004500":
            pipg_byte += plen + ipg_byte
            pipg_bit += plen_bit + ipg_bit
            continue


        if not is_first_processed:
            ipg_byte = 0
            pipg_byte=0
            ipg_bit=0
            pipg_bit=0

        ipg_byte += pipg_byte
        ipg_bit += pipg_bit

        ipd_byte = ipg_byte + prev_plen_byte
        ipd_bit = ipg_bit + prev_plen_bit
        
        pid_block = id_offset / 4 + 4
        pid_offset = id_offset % 4

        pid = int( sep[pid_block][pid_offset*2:] + sep[pid_block+1][0:(4-pid_offset)*2], 16)

        output_file.write('{:12d}\t{:5d}\t{:4d}\t{:8d}\t{:12d}\t{:8d}\t{:12d}\n'.\
                format(pid, ptype, plen, ipg_byte, ipg_bit, ipd_byte, ipd_bit))

        if dump and is_first_processed:
        #	ipgs[ipg_byte] = 1 if ipg_byte not in ipgs else ipgs[ipg_byte]+1
            ipgs_bit[ipg_bit] = 1 if ipg_bit not in ipgs_bit else ipgs_bit[ipg_bit]+1
        #	ipds[ipd_byte] = 1 if ipd_byte not in ipds else ipds[ipd_byte]+1
            ipds_bit[ipd_bit] = 1 if ipd_bit not in ipds_bit else ipds_bit[ipd_bit]+1

        if not is_first_processed:
            is_first_processed = True
            

        if pid != prev_pid +1:
#            print hex(pid), hex(prev_pid)
            lost += pid - prev_pid - 1

        pipg_byte = 0
        pipg_bit = 0

        prev_pid = pid

        prev_plen_byte = plen
        prev_plen_bit = plen_bit

        i += 1
        if i == pkt_cnt:
            break

    input_file.close()
    output_file.close()

    if i != 0 :
        print "Processed {:d} packets, {:d} were lost ({:.2f}%)".format(i, lost, float(lost) / i)
    else:
        print "Processed 0 packets"


    return ipgs_bit, ipds_bit

def check_mac(addr):
    try:
        if len(addr) == 12:
            tmp=int(addr, 16)
            return addr
        elif len(addr) == 17:
            if addr.find(':') == -1:
                return None

            tmp = addr.replace(':','')
            if len(tmp) != 12:
                return None

            rtmp = int(tmp,16)
            return tmp

        elif len(addr) == 14:
            if addr.find('.') == -1:
                return None

            tmp = addr.replace('.','')
            if len(tmp) != 12:
                return None

            rtmp = int(tmp,16)
            return tmp
        else :
            return None

    except ValueError:
        return None

def main():
    global verbose

    parser=argparse.ArgumentParser(description='Parsing sonic captured file',
            formatter_class = argparse.RawDescriptionHelpFormatter, 
            epilog=textwrap.dedent(''' will dump the parsed data with following format: 
    <Packet ID> <Packet type> <Packet Len> <IPG in bytes> <IPG in bits> <IPD in bytes > <IPD in bits>'''))
    parser.add_argument('-i', '--input', type=str,
            help='Input file', required=True)
    parser.add_argument('-o', '--output', type=str,
            help='Output file, default is <input>.info')
    parser.add_argument('-d', '--dump', action='store_true', default=False,
            help='Dump distributions')
    parser.add_argument('-s', '--src', type=str,
            help='Source MAC address', default='201000c28002')
    parser.add_argument('-t', '--dst', type=str,
            help='Destination MAC address', default='0060dd453905')
    parser.add_argument('-f', '--offset', type=int,
            help='Offset to ID', default=42)
    parser.add_argument('-n', '--pkt_cnt', type=int, default=-1,
            help='# of packets to consider')
    parser.add_argument('-v', '--verbose', action='store_true', default=False)

    args=parser.parse_args()

    if args.verbose:
        verbose=True

    if verbose:
        print args

    args.src = check_mac(args.src)
    args.dst = check_mac(args.dst)

    ipgs_bit, ipds_bit = parse_captured_file(args.input, args.output, args.src, args.dst, args.offset, args.pkt_cnt,
            args.dump)

    if args.dump:
#        print "Dumping IPGs in bits to ", args.input+".ipg"
        dump_dist_bits(args.input+".ipg", ipgs_bit)
#        print "Dumping IPDs in bits to ", args.input+".ipd"
        dump_dist_bits(args.input+".ipd", ipds_bit)

if __name__ == "__main__":
    main()
