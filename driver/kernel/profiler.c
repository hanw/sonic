/* based on Intel MSR device driver and PMU profiler */

#if SONIC_KERNEL
#include <asm/processor.h>
#include <asm/msr.h>
#include <asm/system.h>

#include <linux/cpu.h>
#include <linux/smp.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#endif

#include "sonic.h"

struct sonic_msr {
	int fd;
	int cpu;
};

#if SONIC_KERNEL
static uint64_t msr_read(struct sonic_msr *msr, int offset)
{
	int err = 0;
	uint64_t ret;
	uint32_t *p = (uint32_t *) &ret;
	uint32_t *q = p + 1;

	err = rdmsr_safe_on_cpu(msr->cpu, offset, p, q);

	if (err)
		return -1;

	else 
		return ret;
}
#else 
int msr_init(int cpu)
{
	int fd;
	char path[200];
	sprintf(path, "/dev/cpu/%d/msr", cpu);
	
	fd = open(path, O_RDWR);

	if (fd <0)
		return -1;
	
	return fd;
}

static uint64_t msr_read(struct sonic_msr *msr, int offset) {
	uint64_t ret;
	uint32_t result = lseek(msr->fd, offset, SEEK_SET);
	if (result < 0)
		return -1;

	read(msr->fd, (void *) &ret; sizeof(uint64_t));

}
#endif
