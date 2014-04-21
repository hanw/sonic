#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/kobject.h>
#include <linux/kobj_map.h>
#include <linux/module.h>
#include <linux/version.h>

#include "sonic.h"
#include "fifo.h"
#include "ioctl.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
static int sonic_fs_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
		unsigned long arg)
#else
static long sonic_fs_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
#endif
{
	// FIXME
	struct sonic_priv *sonic = file->private_data;
	struct sonic_pkt_gen_info info;
	struct sonic_port *port;
	uint64_t * buf;
	int ret=0;
	SONIC_DDPRINT("sonic = %p\n", sonic);

	if (!sonic) {
		SONIC_ERROR("sonic pointer is null\n");
		return -EFAULT;
	}

	if (_IOC_TYPE(cmd) != SONIC_IOC_MAGIC)
		return -ENOTTY;
	if (_IOC_NR(cmd) > SONIC_IOC_MAXNR)
		return -ENOTTY;
	if (_IOC_DIR(cmd) & _IOC_READ)
		ret = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	if (_IOC_DIR(cmd) & _IOC_WRITE)
		ret = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	if (ret)
		return -EFAULT;

	switch(cmd) {
	case SONIC_IOC_RESET:
		SONIC_DPRINT("SONIC_IOC_RESET\n");
//		sonic_reset_sonic(sonic);

		break;
	case SONIC_IOC_START:
		SONIC_DPRINT("SONIC_IOC_START\n");
		if (sonic_start_sonic(sonic) == -1)
			ret = -EFAULT;

		break;
	case SONIC_IOC_STOP:
		SONIC_DPRINT("SONIC_IOC_STOP\n");
		sonic_stop_sonic(sonic);

		break;
	case SONIC_IOC_RUN:
		SONIC_DPRINT("SONIC_IOC_RUN %ld\n", arg);
		if (sonic_run_sonic(sonic, arg))
			ret = -EFAULT;

		break;
	case SONIC_IOC_SET_MODE:
		SONIC_DPRINT("SONIC_IOC_SET_MODE %ld\n", arg);
		if (sonic_init_sonic(sonic, arg))
			ret = -EFAULT;

		break;
	case SONIC_IOC_GET_STAT:
		SONIC_DPRINT("SONIC_IOC_GET_STAT\n");
		buf = ALLOC(sizeof(struct sonic_stat));
		if (!buf) {
			ret = -ENOMEM;
			break;
		}

		sonic_get_stat(sonic, buf);

		ret = copy_to_user((struct sonic_stat *) arg,buf, sizeof(struct sonic_stat));
		if (ret)
			break;

		break;
	case SONIC_IOC_RESET_STAT:
		SONIC_DPRINT("SONIC_IOC_RESET_STAT\n");
		sonic_reset_stat(sonic);

		break;
	case SONIC_IOC_PORT0_INFO_SET:
	case SONIC_IOC_PORT1_INFO_SET:
		SONIC_DPRINT("SONIC_IOC_PORT_INFO_SET\n");
		ret = copy_from_user(&info, (struct sonic_pkt_gen_info *) arg, sizeof(info));
		if (ret)
			break;

		SONIC_PRINT("mac_src = %s, mac_dst = %s\n",
			info.mac_src, info.mac_dst);

		port = sonic->ports[cmd == SONIC_IOC_PORT0_INFO_SET ? 0 : 1];

		sonic_set_port_info (&port->info, info.mac_src, info.mac_dst, info.ip_src,
				info.ip_dst, info.port_src, info.port_dst);

		// FIXME
//		if (sonic_pkt_gen_set(port->tx_mac, info.mode, info.pkt_len, info.pkt_num, info.idle, info.chain,
//			info.wait)) {
//			ret = -EFAULT;
//			break;
//		}

		break;
	default:
		SONIC_ERROR("Unknown Command\n");
	}

	return ret;
}

static ssize_t 
sonic_fs_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
	struct sonic_logger *logger ;

	struct sonic_log *log = NULL;
	int log_size;
	
	int retval;

	SONIC_DDPRINT("\n");

    if (!file->private_data)
        return -EFAULT;

	if (file->private_data == sonic) 
		return 0;

	logger = file->private_data;
	log_size= order2bytes(logger->log_porder);

	SONIC_DDPRINT("logger count=%u %d\n", (unsigned) count, log_size);

	if (unlikely(count == 0))
		return 0;

	if (unlikely(!access_ok(VERIFY_WRITE, buf, count))) 
		return -EINVAL;

	if (unlikely(count < log_size)) 
		return -EINVAL;

	SONIC_DDPRINT("retrieve\n");

	spin_lock_bh(&logger->lock);
	if (!list_empty(&logger->log_head)) {
		log = list_entry(logger->log_head.next, struct sonic_log, head);
		list_del(&log->head);
	}
	spin_unlock_bh(&logger->lock);
	if (!log) {
		SONIC_DDPRINT("Empty\n");
		return 0;
	}

	retval = log_size;

	SONIC_DDPRINT("Copy to user %d\n", retval);

	if (unlikely(__copy_to_user(buf, log, retval))) 
		retval = -EFAULT;

	FREE_PAGES(log, logger->log_porder);

	SONIC_DDPRINT("retval = %d\n", retval);
	return retval;
}

static ssize_t 
sonic_fs_write(struct file *file, const char __user *buf, 
                size_t count, loff_t *pos)
{
    struct sonic_logger *logger;
	struct list_head * ins_head;

    struct sonic_ins * ins = NULL;
    int ins_size;

    SONIC_DPRINT("\n");

    if (!file->private_data)
        return -EFAULT;

    if (file->private_data == sonic)
        return 0;

    logger = file->private_data;
    ins_size = order2bytes(logger->log_porder);
    ins_head = &logger->ins_head;

    SONIC_DDPRINT("logger count=%u %d\n", (unsigned) count, ins_size);

    if (unlikely(count == 0))
        return 0;

    if (unlikely(!access_ok(VERIFY_READ, buf, count))) 
        return -EINVAL;

    if (unlikely(ins_size < count))
        return -EINVAL;

    SONIC_DDPRINT("copy\n");

    ins = (struct sonic_ins *) ALLOC_PAGES(logger->log_porder);
    if (!ins) {
        SONIC_ERROR("out of memory\n");
        return -ENOMEM;
    }

    if (unlikely(__copy_from_user(ins, buf, count) != 0)) {
        FREE_PAGES(ins, logger->log_porder);
        return -EFAULT;
    }

    spin_lock_bh(&logger->lock);
    list_add_tail(&ins->head, ins_head);
    spin_unlock_bh(&logger->lock);

    return count;
}

static int sonic_fs_open(struct inode *inode, struct file *file)
{
	struct cdev *cdev = inode->i_cdev;
	struct sonic_priv *sonic;
	int cindex = iminor(inode);

	SONIC_DPRINT("cindex = %d\n", cindex);
	if (!cdev) {
		SONIC_ERROR("error\n");
		return -ENODEV;
	}

	if (cindex < 0 || cindex >= SONIC_PORT_SIZE + 1) {
		SONIC_ERROR("Invalid cindex number %d\n", cindex);
		return -ENODEV;
	}

	sonic = container_of(cdev, struct sonic_priv, sonic_cdev);
	// TODO: exclusiveness

	if (cindex == 0) {
		if (atomic_inc_return(&sonic->refcnt) > 1) {
			SONIC_ERROR("too many accesses\n");
			return -EMFILE;
		}
		file->private_data = sonic;
	}
	else {
		if (atomic_inc_return(&sonic->ports[cindex-1]->log->refcnt) > 1) {
			SONIC_ERROR("too many accesses\n");
			return -EMFILE;
		}
		file->private_data = sonic->ports[cindex-1]->log;
	}

	return 0;
}

static int sonic_fs_release(struct inode *inode, struct file *file)
{
	int cindex = iminor(inode);

	SONIC_DPRINT("cindex = %d\n", cindex);

	if (cindex == 0) {
		struct sonic_priv *sonic = file->private_data;
		atomic_dec(&sonic->refcnt);
	} else {
		struct sonic_logger *logger = file->private_data;
		atomic_dec(&logger->refcnt);
	}
	SONIC_DPRINT("\n");
	return 0;
}

static struct file_operations sonic_fops = {
	.open = sonic_fs_open,
	.release = sonic_fs_release,
	.read = sonic_fs_read,
	.write = sonic_fs_write,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
	.ioctl = sonic_fs_ioctl,
#else
	.unlocked_ioctl = sonic_fs_ioctl,
#endif
	.owner = THIS_MODULE,
};

int sonic_fs_init(struct sonic_priv * sonic, int num_ports) 
{
	int status;
	SONIC_PRINT("\n");

	sonic->cdev_minor = num_ports+1;

	status = alloc_chrdev_region(&sonic->sonic_dev, 0, sonic->cdev_minor, 
			SONIC_CHR_DEV_NAME);
	if (status < 0) {
		SONIC_ERROR("alloc_chrdev_region failed %d\n", status);
		return status;
	}

	cdev_init(&sonic->sonic_cdev, &sonic_fops);
	status = cdev_add(&sonic->sonic_cdev, sonic->sonic_dev, sonic->cdev_minor);
	if (status < 0) {
		SONIC_ERROR("cdev_add failed %d\n", status);
		unregister_chrdev_region(sonic->sonic_dev, sonic->cdev_minor);
		return status;
	}

	atomic_set(&sonic->refcnt, 0);

	return 0;
}

void sonic_fs_exit(struct sonic_priv *sonic)
{
	SONIC_PRINT("\n");
	cdev_del(&sonic->sonic_cdev);
	unregister_chrdev_region(sonic->sonic_dev, sonic->cdev_minor);
}

