#! /usr/bin/python

import sys, os, getopt, numpy, math, argparse

bit_width= 1000000000 / 10312500000.0

def sonic_verbose_print(verbose, out):
	if verbose:
		print out

def convert_ipg_to_ipd(pkt_len, pkt_type, idles):
	if pkt_type == 2:
		blk_cnt = pkt_len / 8 
		blk_remaining = pkt_len % 8

		tidles = idles - (8 - blk_remaining)

		ipd = 56 + blk_cnt * 66

	else:
		blk_cnt = (pkt_len - 4) / 8
		blk_remaining = (pkt_len - 4) % 8

		tidles = idles - (8 - blk_remaining)

		ipd = 24 + (blk_cnt + 1)* 66

	idle_cnt = tidles / 8
	idle_remaining = tidles % 8

	if idle_remaining !=4 and idle_remaining != 0:
		print "Error"
		sys.exit(1)

	ipd += idle_cnt * 66 + (10 if idle_remaining == 0 else 42)

	return ipd

def convert_pkt_len_to_bits(pkt_len, pkt_type):
	if pkt_type == 2:
		blk_cnt = pkt_len / 8
		blk_remaining = pkt_len % 8

		pkt_len_bit = 56 + (blk_cnt  - 1)* 66 + blk_remaining * 8 + 10
	else:
		blk_cnt = (pkt_len - 4) / 8
		blk_remaining = (pkt_len - 4) % 8
	
		pkt_len_bit = 24 + (blk_cnt) * 66 + blk_remaining * 8 + 10

	return pkt_len_bit

def dump_dist_bits(fname, dist):
	f = open(fname, 'w')

	total=0
	for key in dist.iterkeys():
		total+=dist[key]


	cdf=0.0
	for key in sorted(dist.iterkeys()):
		p=float(dist[key]) / total
		cdf+=p
		f.write('{0}\t{1}\t{2}\t{3}\t{4}\n'.format(key, float(key) * bit_width, dist[key], p, cdf))

	f.close()

def isbiterror(ipg, interval, pid, msg_bits):
	bit=(pid-1) % len(msg_bits)
	tbit = 1 if ipg < interval else 0

#	print pid, msg_bits[bit], tbit
	return 1 if msg_bits[bit]!=tbit else 0


def compute_regularity_ber(input, window, pkt_cnt, msg_bits, interval, regularity, ber):
#	print "Computing regularity with ", input, " window size ", window

	f=open(input+".info", 'r')

	i=0

	data={}
	l=[]
	fl=[]
	first=0

	berrors=0

	for line in f:
		if line[0]=='#': 
			continue
		if first==0: 
			first=1
			continue

		sep=line.split()
#		data[sep[4]]=[sep[0], sep[1], sep[2], sep[3]]
		i+=1
		ipg=int(sep[3])

		if regularity==1 :
			l.append(ipg)

			if i%100 == 0:
	#			print l, len(l),  numpy.std(l)
				fl.append(numpy.std(l))
	#			print numpy.std(l)
				del l[:]

			if i%10000 == 0:
				print i, " processed"

		# assume no packet loss
		if ber==1:
			berrors=berrors+1 if isbiterror(ipg, interval, int(sep[0]), msg_bits) else berrors

		if pkt_cnt>0 and i >=pkt_cnt :
			break;

	f.close()

	fl2=[]

	if regularity==1:
		print "First phase done"

		for x in fl:
	#		print x, fl[i:]
			for y in fl[i:]:
				fl2.append(math.fabs(x-y) / x)
	#			print x, y
			i+=1
		
		print "Final computation", len(fl2)
					
		print numpy.std(fl2)

	if ber==1:
		print float(berrors) / i

#	for k,v in data.iteritems():
#		print k, v


def compute_similarity(input, epsilon):
#	print "Computing similarity with ", input, " epsilon ", epsilon

	f=open(input+".dist", 'r')

	data={}
	t=0
	total=0
	prev=0
	for line in f:
		sep=line.split()
		data[sep[0]]=sep[1]
		t+=int(sep[1])-1
		total+=int(sep[1])

		print sep[0], sep[1], t, total, prev

		if prev != 0 :
			print math.fabs(int(sep[0]) - prev) / prev
			if (math.fabs(int(sep[0]) - prev) / prev) < epsilon:
				t += 1

		prev=int(sep[0])

	f.close()

	print float(t)/total * 100

def read_dist(fname):
	print "Read dist", fname

	f=open(fname+".dist", 'r')
	total=0
	fmin=0
	fmax=0
	fdata=[]

	for line in f:
		sep=line.split()
		pnum=int(sep[1])
		gap=int(sep[0])
		fdata.append([gap, pnum])
		total+=pnum

		if gap > fmax:
			fmax = gap
		if fmin == 0:
			fmin = gap

	f.close()

	print fmax, fmin, total, fdata

	return fmax, fmin, total, fdata

def normalize_dist(data, total):
	res=[]
	for x in data:
		gap=x[0]
		num=x[1]

		res.append([gap, float(num)/total])

	print res

	return res

def edf(data, t, total):

	res=0
	for x in data:
		gap=x[0]
		num=x[1]
		if gap <= t:
			res += num

	
#	print "edf", res, float(res) / total

	return float(res) / total

# ndata must be sorted.
def cdf(ndata, t):
	res=0.0
	for x in ndata:
		gap=x[0]
		prob=x[1]
		if gap <= t:
			res+= prob

#	print "cdf",  res

	return res;
		

def compute_shape(input, ref):
	print "Computing shape ", input, " and ", ref

	smax, smin, stotal, sdata=read_dist(input)

	sndata=normalize_dist(sdata, stotal)

	fmax, fmin, ftotal, fdata=read_dist(ref)

	fndata=normalize_dist(fdata, ftotal)

	start = min(smin, fmin)
	end = max(smax, fmax)

	sup=0

	for x in range(start, end+1):
		diff=math.fabs(edf(sdata, x, stotal) - cdf(fndata, x))

		if diff > sup:
			sup = diff

	print sup
	
def msg_to_bits(msg):
	r=[]
	o=0
	for i in range(len(msg)* 8):
		byte_offset = i >> 3;
		bit_offset = i & 0x7
	#	print bin(ord(msg[byte_offset])), bit_offset
		r.append((ord(msg[byte_offset]) >> bit_offset) & 0x1)

	#print r

	return r
		
def compute_corrcoeff(input, cnt):
	print "Correlation Coeffieicnt ", input
	f=open(input, 'r')

	i = 0
	odd=[]
	even=[]
	first=0

	plen=0
	plenbit=0


	for line in f:
		sep=line.split()
		ptype=int(sep[0])
		plen = int(sep[1])

		pplenbit = plenbit

		if plen == 1526:
			plenbit= 12590 if ptype==1 else 12588
		elif plen == 72:
			plenbit= 594

		if first==0:
			first=1
			continue

		i+=1

		ipg = int(sep[3])
		ipd = ipg + pplenbit
	
		print ipd

		if i % 4 == 1 or i % 4 == 2:
			odd.append(ipd)
		elif i % 4 == 3 or i % 4 == 0:
			even.append(ipd)
	
		if i==cnt :
			break

	f.close()

#	print i/4, len(odd), len(even)

#	print odd
#	print even

#	print "corrcoeff = ", numpy.corrcoef(odd[:i/2], even[:i/2])
	

def get_dist(l):
	dist={}
	
	for x in l:
		if x in dist:
			dist[x] += 1
		else:
			dist[x] = 1

	return dist
#	print dist
#	for (x,y) in l_dist:
#		print x,y


'''
	Join lines span multiple physical lines into a single line when reading a file line-by-line

	Usage: 
		for lines in joinlines(f.readlines())
'''
def joinlines(raw):
	lines=[]
	for l in raw:
		lines.append(l.strip('\n').strip('\\'))
		if not l.endswith('\\\n'):
			yield ''.join(lines)
			lines=[]
	if len(lines) > 0:
		yield ''.join(lines)

def read_info_file(fname, cnt):
	f = open(fname, 'r')

	ipds=[]
	infos=[]
	i = 0

	first=False
	plen=0
	plentype=1;
	plenbit=0
	pplen = 0
	pplentype=1

	for line in f:
		sep = line.split()
        
		if not first:
			first=True
			continue

		i+=1

		plen = int(sep[2])
		plentype=int(sep[1])
		ipd = int(sep[6])

		if ipd != convert_ipg_to_ipd(pplen, pplentype, int(sep[3])):
			print "Error"
			sys.exit(1)

		ipds.append(int(ipd))
		infos.append([plen, plentype])

		pplen = plen;
		pplentype=plentype

		if i == cnt:
			break

	f.close()

	return ipds, infos

def compute_departure_time(ipds, latency):
	departures=[]
#   departures.append(latency)

	s=latency
	for x in ipds:
		s += x
		departures.append(s)

	return departures

def compute_waiting_idle_time(arrivals, departures):
	waitings=[]
	idles=[]
	services=[]

	waitings.append(0)
	idles.append(0)
	services.append(departures[0] - arrivals[0])

	d=0

	for i in range(len(arrivals)- 1):
		n_arrival = arrivals[i+1]

		c_departure = departures[i]
		n_departure = departures[i+1]

		if n_arrival < c_departure:
			waitings.append(c_departure - n_arrival)
			idles.append(0)
			services.append(n_departure - c_departure)
		else:
			waitings.append(0)
			idles.append (n_arrival - c_departure)
			services.append(n_departure - n_arrival)

	return waitings, idles, services


