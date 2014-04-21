#include <asm/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/module.h>

#include "sonic.h"
#include "tester.h"

static char *sonic_procfile = "sonic_tester";
module_param(sonic_procfile, charp, 0000);
MODULE_PARM_DESC(sonic_procfile, "Path to /proc/<file>");

/* from Kernel to Userspace */
static int sonic_proc_read(char *buf, char **start, off_t off, int count,
		int *eof, void *data)
{
	struct sonic_priv * sonic = (struct sonic_priv *) data;
	int len;
	if (!sonic->procpage)
		return 0;

	if (count > PAGE_SIZE)
		return -EINVAL;

	len = strlen(sonic->procpage);

	strncpy(buf, sonic->procpage, len);

	len -= off;
	if (len < count) {
		*eof = 1;
		if (len <= 0)
			return 0;
	} else
		len = count;
	*start = buf + off;
	return len;
}

/* from Userspace to Kernel */
static int sonic_proc_write(struct file *filp, const char __user *buff, 
		unsigned long count, void *data)
{
	struct sonic_priv * sonic = (struct sonic_priv *) data;
	char *page;
	char buf[128];

	if (count > 128)
		return -EINVAL;

	if (copy_from_user(buf, buff, count))
		return -EFAULT;

	buf[count] = '\0';

	if (!strncmp(buf, "tester ", 7)) {
		SONIC_PRINT("tester\n");
		sonic_test_starter(sonic, buf);
	} else if (!strncmp(buf, "start ", 6)) {
		int mode;

		sscanf(buf + 6, "%d", &mode);

		SONIC_PRINT("Starting sonic %d\n", mode);

		set_cpus_allowed(current, cpumask_of_cpu(6));

		sonic_init_sonic(sonic, mode);

		sonic_start_sonic(sonic);
		
	} else if (!strncmp(buf, "stop", 4)) {
		SONIC_PRINT("Stopping sonic\n");
		sonic_stop_sonic(sonic);

	} else if (!strncmp(buf, "stat", 4)) {
		SONIC_PRINT("Getting stat\n");
		page = (char *) ALLOC_PAGE();
		sonic_print_stat(sonic, page);

		SONIC_PRINT("%s\n", page);

		FREE_PAGE(page);
	} else if (!strncmp(buf, "reset", 5)) {
		SONIC_PRINT("Resetting stat\n");
		sonic_reset_stat(sonic);
	} else if (!strncmp(buf, "test", 4)) {
		int ret;
		SONIC_PRINT("..");
		sonic_fpga_reset(sonic->ports[0]);
		sonic_fpga_reset(sonic->ports[1]);
		if((ret= sonic_send_cmd(sonic->ports[0], SONIC_CMD_GET_RX_OFFSET, 0, 0, 0)) == -1)
			SONIC_PRINT("ERROR..RXOFFSET\n");
		sonic->ports[0]->rx_offset=ret;
		if((ret= sonic_send_cmd(sonic->ports[0], SONIC_CMD_GET_RX_SIZE, 0, 0, 0)) == -1) 
			SONIC_PRINT("ERROR..RXSIZE\n");
		sonic->ports[0]->rx_size=ret;
		if((ret= sonic_send_cmd(sonic->ports[0], SONIC_CMD_SET_RX_BLOCK_SIZE, 4096>>2, 0, 0)) == -1)
			SONIC_PRINT("ERROR..SETBLOCKSIZE\n");
	}
	
	return count;
}

int sonic_proc_init(struct sonic_priv * sonic)
{
	SONIC_PRINT("\n");
	if (!(sonic->procpage = (void *) get_zeroed_page(GFP_KERNEL))) {
		SONIC_ERROR(" unable to allocate a page \n");
		return -ENOMEM;
	}
	* (char *) sonic->procpage = '\0';
	sonic->procfile = sonic_procfile;

	if (!(sonic->procentry = create_proc_entry(sonic->procfile, 0666, 
			NULL))) {
		SONIC_ERROR(" unable to create proc file %s\n", sonic->procfile);
		free_page((unsigned long) sonic->procpage);
		return -EINVAL;
	}

	sonic->procentry->data = sonic;
	sonic->procentry->write_proc = sonic_proc_write;
	sonic->procentry->read_proc = sonic_proc_read;
	SONIC_PRINT("proc init successed\n");
	return 0;
}

void sonic_proc_exit(struct sonic_priv * sonic)
{
	SONIC_PRINT("\n");
	if (sonic->procentry)
		remove_proc_entry(sonic->procfile, NULL);

	if (sonic->procpage)
		free_page((unsigned long) sonic->procpage);
}
