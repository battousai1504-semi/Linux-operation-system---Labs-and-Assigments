#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/errno.h>

#include "asgn1_ioctl.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tin");
MODULE_DESCRIPTION("ASGN1_ioctl driver");

#define BUFFER_SIZE 1024
#define DEVICE_NAME "asgn1"

static int major = 240 ;

static char buffer[BUFFER_SIZE];
static int buffer_len;

static int open_count;
static int write_count;
static int read_count;
static int last_write_size;

static int mode = 1;

static DEFINE_MUTEX(buffer_mutex);

static int asgn1_open(struct inode *inode, struct file *file)
{
    mutex_lock(&buffer_mutex);
    open_count++;

    pr_info("asgn1: device opened, open_count=%d\n",
            open_count);

    mutex_unlock(&buffer_mutex);

    return 0;
}

static ssize_t asgn1_read(struct file *file,
                          char __user *user_buf,
                          size_t count,
                          loff_t *ppos)
{
    size_t bytes_to_read;
    size_t available;

    mutex_lock(&buffer_mutex);

    if (*ppos >= buffer_len) {
        mutex_unlock(&buffer_mutex);
        return 0;
    }

    available = buffer_len - *ppos;
    bytes_to_read = min(count, available);

    if (copy_to_user(user_buf,
                     buffer + *ppos,
                     bytes_to_read)) {
        mutex_unlock(&buffer_mutex);
        return -EFAULT;
    }

    *ppos += bytes_to_read;
    read_count++;

    mutex_unlock(&buffer_mutex);

    return bytes_to_read;
}

static ssize_t asgn1_write(struct file *file,
                           const char __user *user_buf,
                           size_t count,
                           loff_t *ppos)
{
    size_t bytes_to_write;
    size_t prefix_len = 0;
    char *temp;
    const char prefix[] = "[KERNEL]";

    if (count == 0)
        return 0;
    bytes_to_write = min(count, (size_t)BUFFER_SIZE);
    temp = kmalloc(bytes_to_write, GFP_KERNEL);
    if (!temp)
        return -ENOMEM;

    if (copy_from_user(temp, user_buf, bytes_to_write)) {
        kfree(temp);
        return -EFAULT;
    }

    mutex_lock(&buffer_mutex);

    if (mode == 0) {
        prefix_len = sizeof(prefix) - 1;

        bytes_to_write = min(bytes_to_write, (size_t)(BUFFER_SIZE - prefix_len));

        memmove(buffer + prefix_len, temp, bytes_to_write);
        memcpy(buffer, prefix, prefix_len);

        buffer_len = prefix_len + bytes_to_write;
    } else {
        memcpy(buffer, temp, bytes_to_write);
        buffer_len = bytes_to_write;
    }

    write_count++;
    last_write_size = bytes_to_write;

    mutex_unlock(&buffer_mutex);
    kfree(temp);

    return bytes_to_write;
}

static long asgn1_ioctl (struct file *file,
                         unsigned int cmd,
                         unsigned long arg)
{
    int ret = 0;

    mutex_lock(&buffer_mutex);

    switch (cmd) {
        case ASGN1_RESET_BUFFER:
            memset(buffer, 0, BUFFER_SIZE);
            buffer_len = 0;
            write_count = 0;
            read_count = 0;
            last_write_size = 0;

            pr_info("asgn1: buffer reset\n");
            break;

        case ASGN1_GET_STATS: {
            struct asgn1_stats stats;

            pr_info("asgn1: GET_STATS ioctl called\n");

            stats.open_count = open_count;
            stats.write_count = write_count;
            stats.read_count = read_count;
            stats.buffer_len = buffer_len;
            stats.last_write_size = last_write_size;

            if (copy_to_user((void __user *)arg,
                            &stats,
                            sizeof(stats))) {
                ret = -EFAULT;
            }

            break;
        }
        case ASGN1_SET_MODE: {
            int new_mode;

            if (copy_from_user(&new_mode,
                               (int __user *)arg,
                                sizeof(new_mode))) {
                ret = -EFAULT;
                break;
            }

            if (new_mode != 0 && new_mode != 1) {
                ret = -EINVAL;
                break;
            }

            mode = new_mode;
            pr_info("asgn1: mode changed to %d\n",mode);
            break;
        }

        case ASGN1_GET_VERSION: {
            char version[64] = "ASGN1 Driver Version 1.0";

            pr_info("asgn1: GET_VERSION ioctl called\n");

            if (copy_to_user((char __user *)arg,
                            version,
                            sizeof(version))) {
                ret = -EFAULT;
            }
            break;
        }

        default:
            ret = -ENOTTY;
            break;
    }

    mutex_unlock(&buffer_mutex);

    return ret;
}

static const struct file_operations asgn1_fops = {
    .owner = THIS_MODULE,
    .open = asgn1_open,
    .read = asgn1_read,
    .write = asgn1_write,
    .unlocked_ioctl = asgn1_ioctl,
};

static int __init asgn1_init(void)
{
    int ret;

    ret = register_chrdev(major, DEVICE_NAME, &asgn1_fops);

    if (ret < 0) {
        pr_err("asgn1: failed to register device\n");
        return ret;
    }

    pr_info("asgn1: registered with major number %d\n", major);

    return 0;
}

static void __exit asgn1_exit(void)
{
    unregister_chrdev(major, DEVICE_NAME);
    pr_info("asgn1: device unregistered\n");
}

module_init(asgn1_init);
module_exit(asgn1_exit);