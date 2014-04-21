from numpy import *
import argparse, sys, os

verbose=False

def generate_cross_traffic(mode, rate, q, p, n):
    """Generate packet size and number of idle bytes in between packets.
    
    Input arguments:
    mode: Specifies the type of packet sizes and idles to generate. Three possible modes (1, 2 or 3)
        mode 1 - variable packet length but fixed interpacket gap
        mode 2 - fixed packet length but variable interpacket gap
        mode 3 - variable packet length and variable interpacket gap
    rate: Data rate of cross traffic in bytes
    q: a value within [0,1], this is the tuning knob for the variance of the packet size.
        q = 0 - zero variance
        q = 1 - maximum variance as specified in the constants section
        0 < q < 1 - implies a variance of q * maximum variance
    p: a value within [0,1], this is the parameter for the geometric distribution used to generate the number of packets within a packet chain
        p = 1 - chain length equals to 1 packet always
        p < 1 - chain length equals to 1 packet with probability p, 2 packets with probability p(1-p), ... , k packets with probability p(1-p)^(k-1)
        The expected chain length is equal to 1/p so the larger p is, the shorter the chain length is expected to be and vice versa
        It is recommended that p to be above 0.1
    n: number of packets to be generated

    Packet size generation:
    The packet size follows a lognormal distribution with parameter u and s. The mean and variance of the lognormal distribution is given by
    mean, m  = exp(u +  s^2/2)
    variance, v = [exp(s^2) - 1]exp(2u + s^2)
    Solving for u and s in terms of mean and variance give
    u = log(m^2/sqrt(v + m^2))
    s = sqrt(log(1 + v/m^2))

    Interpacket gap generation:
    The interpacket gap does not follow a distribution but is instead constructed in the following manner
    1) First draw a value from Geo(p) (with support {1,2,3, ...})
    2) The value drawn is the number of packets in the packet chain. Set interpacket gap to zero for packets within the packet chain.
    3) Allocate enough idle space after the chain such that the desired data rate is achieved
    4) Repeat steps 1 - 3 till the desired number of packets is generated
    """
    # Constants
    capacity = 10e9/8 # capacity in bytes;
    #max_variance = 8e4 # maximum variance in bytes^2, 
    max_variance = 8e4 # maximum variance in bytes^2, 
    mean_packet_size = 500 # mean packet size in bytes
    
    # Compute u and s^2
    m = mean_packet_size
    v = q * max_variance
    u = math.log(m**2/math.sqrt(v + m**2))
    s = math.sqrt(math.log(1 + v/m**2))

    # initialize packet size vector
    packet_size = mean_packet_size * ones(n)

    # generate variable packet length
    if mode == 1 or mode == 3:
        # generate n lognormal values
        for i in range(1,n):
            p_size = 0
            while (p_size < 72 or p_size > 1526):
                p_size = round(random.lognormal(u, s))
            packet_size[i-1] = p_size
    
    # Mean packet size after discarding packets that are too small or too big
    realized_mean_packet_size = mean(packet_size)
                    
    # Compute average interpacket gap in bytes
    if mode == 2: # mode 2 is the only mode where packet length is fixed
        mean_ipd = round(mean_packet_size * capacity / rate)
        mean_ipg = mean_ipd - mean_packet_size
    else:
        mean_ipd = round(realized_mean_packet_size * capacity / rate)
        mean_ipg = mean_ipd - realized_mean_packet_size  
    
    # initialize idle vector
    idle = mean_ipg * ones(n)  

    # generate variable interpacket gap
    if mode == 2 or mode  == 3:
        k = 0 # keep track of number of packets generated
        while k <= n:
            if k == 0: # first idle
                chain_length = 1 
            else:
                chain_length = random.geometric(p)

            # set zero interpacket gap for
            for j in range(1, chain_length):
                if k+j-1 < n:
                    idle[k+j-1] = 0

            # set sufficient interpacket gap between the chain and the next packet to achieve the desired data rate
            mean_ipg_to_set = mean_ipg * chain_length 

            if k+chain_length-1 < n:
                idle[k+chain_length-1] = mean_ipg_to_set # we might change this to include some randomness later

            k = k + chain_length

    
    # Total time in bytes
    stream_length = sum(idle) + sum(packet_size)

    # Generated data rate
    generated_rate = capacity * sum(packet_size)/(sum(idle) + sum(packet_size))

    return (idle, packet_size, stream_length, generated_rate)

idle, packet_size, stream_length, generated_rate = generate_cross_traffic(3, 3e9/8, 1, 0.7, 1000000)

#print stream_length, generated_rate * 8

def main():
    global verbose

    parser=argparse.ArgumentParser(description='generating random patterns of packets',
            formatter_class = argparse.RawDescriptionHelpFormatter);
    parser.add_argument('-v', '--verbose', action='store_true', default=False)

    parser.add_argument('-m', '--mode', type=int, default=1)
    parser.add_argument('-r', '--rate', default=3e9/8, type=float)
    parser.add_argument('-q', '--variance', default=1, type=float)
    parser.add_argument('-p', '--param', default=0.7, type=float)
    parser.add_argument('-t', '--time', default=1, type=int)
    parser.add_argument('-o', '--output', type=str)

    args=parser.parse_args()

    if args.verbose:
        verbose=True

    if verbose:
        print args

    args.rate=args.rate * 1000000000 / 8.0

    # estimate time first
    (idle, packet_size, length, rate) = generate_cross_traffic(args.mode, args.rate, args.variance, args.param, 10000)

#    print length / (10e9/8), rate * 8

    # new number of packets
    new_n = float(args.time) / (length / (10e9/8)) * 10000

    if new_n < 1: 
        new_n = 1

#    print new_n

    (idle, packet_size, length, rate) = generate_cross_traffic(args.mode, args.rate, args.variance, args.param, int(new_n))

#    print length / (10e9/8), rate * 8

    if args.output is not None:
        f=open(args.output, 'w')
    else:
        f=sys.stdout

    for i in range(len(idle)):
        f.write("{} {} {} {}\n".format(i, 1, int(packet_size[i]), int(idle[i])))

    if args.output is not None:
        f.close()

    print int(new_n),  length / (10e9/8), rate * 8

# sanity check, you might want to use to reject generated traffic if it's too far away from the intended rate
#generated_rate_in_bits = generated_rate * 8
#print('%.4e' %generated_rate_in_bits )

if __name__ == "__main__":
    main()
