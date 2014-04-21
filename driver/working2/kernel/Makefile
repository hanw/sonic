SONIC_DEBUG=1
SONIC_DDEBUG=0
SONIC_DMEMORY=0
SONIC_PROC=1
SONIC_FS=1
SONIC_PCIE=1
SONIC_SFP=1
SONIC_PROFILER=0
SONIC_DMA_POINTER=1
SONIC_FAST_CRC=1
SONIC_VLAN=1
SONIC_SYNCH=0

DMA_ITERATIONS=50

SONIC_DISABLE_IRQ=1

SONIC_TWO_PORTS=1

SONIC := sonic
SONIC_OBJS := config.o fifo.o kernel.o port.o util.o run.o mode.o pcs.o mac.o\
        app.o crc.o stat.o
#SONIC_OBJS := kernel.o crc.o tx_pcs.o rx_pcs.o mac.o util.o port.o thread.o \
#        fifo.o stat.o covert_tx_pcs.o covert_rx_pcs.o app.o config.o availbw.o

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
SONIC_SYNCH=0
endif
ifeq ($(SONIC_PCIE), 0)
SONIC_SFP=0
endif

EXTRA_CFLAGS += -DSONIC_KERNEL=$(SONIC_KERNEL)
EXTRA_CFLAGS += -DSONIC_DEBUG=$(SONIC_DEBUG)
EXTRA_CFLAGS += -DSONIC_DDEBUG=$(SONIC_DDEBUG)
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
EXTRA_CFLAGS += -DSONIC_VLAN=$(SONIC_VLAN)
EXTRA_CFLAGS += -DSONIC_SYNCH=$(SONIC_SYNCH)

EXTRA_CFLAGS += -DSONIC_SCRAMBLER=$(SONIC_SCRAMBLER)
EXTRA_CFLAGS += -DDMA_ITERATIONS=$(DMA_ITERATIONS)
EXTRA_CFLAGS += -DSONIC_FAST_CRC=$(SONIC_FAST_CRC)

ifeq ($(SONIC_PCIE), 1)
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

all: default tester 
#all: default tester 

default:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules

clean: 
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) clean
	rm -f *.o tester sonic_user sonic_cap sonic_rpt

# handling driver
load: sonic.ko
	sudo rmmod sonic; sudo insmod sonic.ko

unload: sonic.ko
	sudo rmmod sonic
endif
CC = gcc -Wall

tester: tester_user.c \
		app.c \
		crc.c \
		crc32.c crc32.h \
		config.c config.h \
		fifo.c \
		mac.c \
		mode.c \
		pcs.c \
		port.c \
		run.c \
		stat.c \
		util.c \
		tester.c 
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $(EXTRA_CFLAGS) $^ -lrt -lpthread

#sonic_user: sonic_user.c stat.c
#	$(CC) -o $@ -DSONIC_TWO_PORTS $^

#sonic_cap: sonic_cap.c

#sonic_rpt: sonic_rpt.c

#sonic_crpt: sonic_crpt.c
