#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/kobject.h>
#include <linux/kobj_map.h>
#include <linux/module.h>
#include <linux/version.h>

#include "sonic.h"
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
    //struct sonic_port *port;
    struct sonic_config_runtime_args *args = &sonic->runtime_args;
//    uint64_t * buf;
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
        SONIC_PRINT("SONIC_IOC_RESET\n");
        __sonic_reset(sonic);       // FIXME: MUTEX
        break;
/*
    case SONIC_IOC_START:
        SONIC_DPRINT("SONIC_IOC_START\n");
        if (sonic_start_sonic(sonic) == -1)
            ret = -EFAULT;
        break;
    case SONIC_IOC_STOP:
        SONIC_DPRINT("SONIC_IOC_STOP\n");
        sonic_stop_sonic(sonic);
        break;
*/

    case SONIC_IOC_RUN:
        SONIC_PRINT("SONIC_IOC_RUN %ld\n", arg);
        sonic->runtime_args.duration=arg;
#if SONIC_DEBUG
        sonic_print_config_runtime_args(args);
#endif
        if (__sonic_run(sonic))
            ret = -EFAULT;
        break;

    case SONIC_IOC_SET_MODE:
        SONIC_PRINT("SONIC_IOC_SET_MODE %ld\n", arg);
        //sonic->runtime_args.mode = arg; // FIXME
        break;

    case SONIC_IOC_GET_STAT:
        SONIC_PRINT("SONIC_IOC_GET_STAT\n");
//        buf = ALLOC(sizeof(struct sonic_sonic_stat));
//        if (!buf) {
//            ret = -ENOMEM;
//            break;
//        }

//        sonic_copy_stats(sonic, (struct sonic_sonic_stat *) buf);
        ret = copy_to_user((struct sonic_sonic_stat *)arg, 
                &sonic->stat, sizeof(struct sonic_sonic_stat));
//        FREE(buf);
        break;

    case SONIC_IOC_RESET_STAT:
        SONIC_PRINT("SONIC_IOC_RESET_STAT\n");
        //sonic_reset_stat(sonic);

        break;
    case SONIC_IOC_PORT0_INFO_SET:
        SONIC_PRINT("SONIC_IOC_PORT0_INFO_SET\n");
        ret = copy_from_user(&args->ports[0], (struct sonic_config_runtime_port_args *) arg, 
                sizeof(struct sonic_config_runtime_port_args));
        break;
    case SONIC_IOC_PORT1_INFO_SET:
        SONIC_PRINT("SONIC_IOC_PORT1_INFO_SET\n");
        ret = copy_from_user(&args->ports[1], (struct sonic_config_runtime_port_args *) arg, 
                sizeof(struct sonic_config_runtime_port_args));
        break;

    default:
        SONIC_ERROR("Unknown Command\n");
    }

    return ret;
}

/* From kernel to userspace */
static ssize_t 
sonic_fs_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
    struct sonic_app *app ;

    struct sonic_app_list *app_list = NULL;
    int app_list_size;

    int retval;

    SONIC_DDPRINT("\n");

    if (!file->private_data)
        return -EFAULT;

    if (file->private_data == sonic) 
        return 0;

    app = file->private_data;
    app_list_size= order2bytes(app->app_exp);

    SONIC_DDPRINT("app count=%u %d\n", (unsigned) count, app_list_size);

    if (unlikely(count == 0))
        return 0;

    if (unlikely(!access_ok(VERIFY_WRITE, buf, count))) 
        return -EINVAL;

    if (unlikely(count < app_list_size)) 
        return -EINVAL;

    SONIC_DDPRINT("retrieve\n");

    spin_lock_bh(&app->lock);
    if (!list_empty(&app->out_head)) {
        app_list = list_entry(app->out_head.next, struct sonic_app_list, head);
        list_del(&app_list->head);
    }
    spin_unlock_bh(&app->lock);
    if (!app_list) {
        SONIC_DDPRINT("Empty\n");
        return 0;
    }

    retval = app_list_size;

    SONIC_DDPRINT("Copy to user %d\n", retval);

    if (unlikely(__copy_to_user(buf, app_list, retval))) 
        retval = -EFAULT;

    FREE_PAGES(app_list, app->app_exp);

    SONIC_DDPRINT("retval = %d\n", retval);
    return retval;
}

/* From userspace to kernel */
static ssize_t 
sonic_fs_write(struct file *file, const char __user *buf, 
                size_t count, loff_t *pos)
{
    struct sonic_app *app;
    struct list_head * rpt_head;

    struct sonic_app_list * app_list = NULL;
    int app_list_size;

    SONIC_DPRINT("\n");

    if (!file->private_data)
        return -EFAULT;

    if (file->private_data == sonic)
        return 0;

    app = file->private_data;
    app_list_size = order2bytes(app->app_exp);
    rpt_head = &app->in_head;

    SONIC_DPRINT("app count=%u %d\n", (unsigned) count, app_list_size);

    if (unlikely(count == 0))
        return 0;

    if (unlikely(!access_ok(VERIFY_READ, buf, count))) 
        return -EINVAL;

    if (unlikely(app_list_size < count))
        return -EINVAL;

    SONIC_DDPRINT("copy\n");

    app_list = (struct sonic_app_list *) ALLOC_PAGES(app->app_exp);
    if (!app_list) {
        SONIC_ERROR("out of memory\n");
        return -ENOMEM;
    }

    if (unlikely(__copy_from_user(app_list, buf, count) != 0)) {
        FREE_PAGES(app_list, app->app_exp);
        return -EFAULT;
    }

    spin_lock_bh(&app->lock);
    list_add_tail(&app_list->head, rpt_head);
    spin_unlock_bh(&app->lock);

    return count;
}

static int sonic_fs_open(struct inode *inode, struct file *file)
{
    struct cdev *cdev = inode->i_cdev;
    struct sonic_priv *sonic;
    int cindex = iminor(inode);

    SONIC_DDPRINT("cindex = %d\n", cindex);
    if (!cdev) {
        SONIC_ERROR("error\n");
        return -ENODEV;
    }

    if (cindex < 0 || cindex >= SONIC_PORT_SIZE + 1) {
        SONIC_ERROR("Invalid cindex number %d\n", cindex);
        return -ENODEV;
    }

    sonic = container_of(cdev, struct sonic_priv, sonic_cdev);

    // TODO: Mutex?
    // Control Port
    if (cindex == 0) {
        if (atomic_inc_return(&sonic->refcnt) > 1) {
            SONIC_ERROR("too many accesses\n");
            atomic_dec(&sonic->refcnt);
            return -EMFILE;
        }
        file->private_data = sonic;
    }
    // Data Ports
    else {
        if (atomic_inc_return(&sonic->ports[cindex-1]->app->refcnt) > 1) {
            SONIC_ERROR("too many accesses\n");
            atomic_dec(&sonic->ports[cindex-1]->app->refcnt);
            return -EMFILE;
        }
        file->private_data = sonic->ports[cindex-1]->app;
    }

    return 0;
}

static int sonic_fs_release(struct inode *inode, struct file *file)
{
    int cindex = iminor(inode);

    SONIC_DDPRINT("cindex = %d\n", cindex);

    // Control Port
    if (cindex == 0) {
        struct sonic_priv *sonic = file->private_data;
        atomic_dec(&sonic->refcnt);
    // Data Ports
    } else {
        struct sonic_app *app = file->private_data;
        atomic_dec(&app->refcnt);
    }

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
    SONIC_DPRINT("\n");

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
        goto err_region;
    }

    // TODO: Create dev files

    atomic_set(&sonic->refcnt, 0);

    return 0;

err_region:
    unregister_chrdev_region(sonic->sonic_dev, sonic->cdev_minor);
    return status;

}

void sonic_fs_exit(struct sonic_priv *sonic)
{
    SONIC_DPRINT("\n");

    cdev_del(&sonic->sonic_cdev);

    unregister_chrdev_region(sonic->sonic_dev, sonic->cdev_minor);
}

