SONIC_DEBUG=1
SONIC_DDEBUG=0
SONIC_DMEMORY=0
SONIC_TESTER=1
SONIC_PROC=1
SONIC_FS=1
SONIC_PCIE=1
SONIC_SFP=1
SONIC_PROFILER=0
SONIC_DMA_POINTER=1

DMA_ITERATIONS=50

SONIC_DISABLE_IRQ=1

SONIC_TWO_PORTS=1
SONIC_PCS_LEVEL=1

# scrambler/descrambler optimize level
SONIC_SCRAMBLER=0	# 0 : optimized, 1 : bitwise
# crc optimize level
SONIC_CRC=0	# 0 : fastcrc, 1 : lookup table 2: bitwise

SONIC := sonic
SONIC_OBJS := kernel.o crc.o tx_pcs.o rx_pcs.o mac.o util.o port.o thread.o \
        fifo.o stat.o covert_tx_pcs.o covert_rx_pcs.o app.o config.o availbw.o

EXTRA_CFLAGS = -O3

# Do not modify this
SONIC_KERNEL=1
# For userlevel these must be off
ifeq ($(KERNELRELEASE),)
SONIC_PCIE=0
SONIC_PROC=0
SONIC_FS=0
SONIC_SFP=0
SONIC_DISABLE_IRQ=0
SONIC_KERNEL=0
SONIC_PROFILER=0		# For now
endif

EXTRA_CFLAGS += -DSONIC_KERNEL=$(SONIC_KERNEL)
EXTRA_CFLAGS += -DSONIC_DEBUG=$(SONIC_DEBUG)
EXTRA_CFLAGS += -DSONIC_DDEBUG=$(SONIC_DDEBUG)
EXTRA_CFLAGS += -DSONIC_TESTER=$(SONIC_TESTER)
EXTRA_CFLAGS += -DSONIC_PCIE=$(SONIC_PCIE)
EXTRA_CFLAGS += -DSONIC_PROC=$(SONIC_PROC)
EXTRA_CFLAGS += -DSONIC_FS=$(SONIC_FS)
EXTRA_CFLAGS += -DSONIC_TWO_PORTS=$(SONIC_TWO_PORTS)
EXTRA_CFLAGS += -DSONIC_SFP=$(SONIC_SFP)
EXTRA_CFLAGS += -DSONIC_DISABLE_IRQ=$(SONIC_DISABLE_IRQ)
EXTRA_CFLAGS += -DSONIC_PCS_LEVEL=$(SONIC_PCS_LEVEL)
EXTRA_CFLAGS += -DSONIC_DMEMORY=$(SONIC_DMEMORY)
EXTRA_CFLAGS += -DSONIC_PROFILER=$(SONIC_PROFILER)
EXTRA_CFLAGS += -DSONIC_DMA_POINTER=$(SONIC_DMA_POINTER)

EXTRA_CFLAGS += -DSONIC_SCRAMBLER=$(SONIC_SCRAMBLER)
EXTRA_CFLAGS += -DSONIC_CRC=$(SONIC_CRC)
EXTRA_CFLAGS += -DDMA_ITERATIONS=$(DMA_ITERATIONS)

ifeq ($(SONIC_TESTER), 1)
	SONIC_OBJS += tester.o
	SONIC_OBJS += tester_helper.o
endif
ifeq ($(SONIC_SFP), 1)
	SONIC_OBJS += hardware.o
endif 
ifeq ($(SONIC_PROC), 1)
	SONIC_OBJS += proc.o 
endif
ifeq ($(SONIC_FS), 1)
	SONIC_OBJS += fs.o
endif

ifeq ($(SONIC_PROFILER), 1)
	SONIC_OBJS += profiler.o
endif

ifneq ($(KERNELRELEASE),)
# in Kernel
obj-m := $(SONIC).o
$(SONIC)-objs := $(SONIC_OBJS) 

else
KVER := $(shell uname -r)
KDIR := /lib/modules/$(KVER)/build
KSRC := /lib/modules/$(KVER)/source
PWD := $(shell pwd)

#all: tester sonic_user sonic_cap sonic_ins
all: default tester sonic_user sonic_cap sonic_ins

default:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules

clean: 
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) clean
	rm -f *.o tester sonic_user sonic_cap sonic_ins

# handling driver
load: sonic.ko
	sudo rmmod sonic; sudo insmod sonic.ko

unload: sonic.ko
	sudo rmmod sonic
endif
CC = gcc -lrt -Wall

tester: tester_user.c crc32.c crc32.h crc.c port.c tx_pcs.c rx_pcs.c mac.c util.c thread.c tester.c tester.h \
	fifo.c fifo.h tester_helper.c sonic.h covert.h covert_tx_pcs.c covert_rx_pcs.c stat.c app.c mode.h config.c\
    availbw.c
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $(EXTRA_CFLAGS) $^ 

sonic_user: sonic_user.c

sonic_cap: sonic_cap.c

sonic_ins: sonic_ins.c

