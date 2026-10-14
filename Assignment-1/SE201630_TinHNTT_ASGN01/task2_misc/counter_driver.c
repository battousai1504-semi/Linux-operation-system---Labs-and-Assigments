
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/mutex.h>

#define DEVICE_NAME "counter"
#define PROC_NAME "counter_info"

static atomic_t counter = ATOMIC_INIT(0);
static atomic64_t total_reads = ATOMIC64_INIT(0);
static atomic64_t total_writes = ATOMIC64_INIT(0);
static DEFINE_MUTEX(counter_lock);

static ssize_t counter_read(struct file *file,
                            char __user *ubuf,
                            size_t count,
                            loff_t *ppos)
{
    char buf[32];
    int value;
    int len;

    mutex_lock(&counter_lock);

    if (*ppos > 0) {
        mutex_unlock(&counter_lock);
        return 0;
    }

    value = atomic_read(&counter);
    len = scnprintf(buf, sizeof(buf), "%d\n", value);

    if (count < len) {
        mutex_unlock(&counter_lock);
        return -EINVAL;
    }

    if (copy_to_user(ubuf, buf, len)) {
        mutex_unlock(&counter_lock);
        return -EFAULT;
    }

    atomic_inc(&counter);
    atomic64_inc(&total_reads);
    *ppos += len;

    mutex_unlock(&counter_lock);

    return len;
}

static ssize_t counter_write(struct file *file,
                             const char __user *ubuf,
                             size_t count,
                             loff_t *ppos)
{
    char *kbuf;
    int value;
    int ret;


    
    if (count == 0)
        return 0;

    if (count >= 64)
        return -EINVAL;

    kbuf = kmalloc(count + 1, GFP_KERNEL);
    if (!kbuf)
        return -ENOMEM;

    if (copy_from_user(kbuf, ubuf, count)) {
        kfree(kbuf);
        return -EFAULT;
    }

    kbuf[count] = '\0';

    ret = kstrtoint(kbuf, 10, &value);
    kfree(kbuf);

    if (ret < 0)
        return ret;

    if (value < 0)
        return -EINVAL;

    mutex_lock(&counter_lock);
        
    atomic_set(&counter, value);
    atomic64_inc(&total_writes);

    mutex_unlock(&counter_lock);

    return count;
}

static int counter_info_show(struct seq_file *m, void *v)
{
    seq_printf(m, "Current counter: %d\n",
               atomic_read(&counter));
    seq_printf(m, "Total reads: %lld\n",
               (long long)atomic64_read(&total_reads));
    seq_printf(m, "Total writes: %lld\n",
               (long long)atomic64_read(&total_writes));

    return 0;
}

static int counter_info_open(struct inode *inode, struct file *file)
{
    return single_open(file, counter_info_show, NULL);
}

static const struct proc_ops counter_proc_ops = {
    .proc_open = counter_info_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static const struct file_operations counter_fops = {
    .owner = THIS_MODULE,
    .read = counter_read,
    .write = counter_write,
    .llseek = no_llseek,
};

static struct miscdevice counter_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = DEVICE_NAME,
    .fops = &counter_fops,
    .mode = 0666,
};

static struct proc_dir_entry *counter_proc_entry;

static int __init counter_init(void)
{
    int ret;

    ret = misc_register(&counter_device);
    if (ret < 0) {
        pr_err("counter: failed to register misc device\n");
        return ret;
    }

    counter_proc_entry = proc_create(
        PROC_NAME, 0444, NULL, &counter_proc_ops);

    if (!counter_proc_entry) {
        misc_deregister(&counter_device);
        pr_err("counter: failed to create proc entry\n");
        return -ENOMEM;
    }

    pr_info("counter: module loaded\n");
    return 0;
}

static void __exit counter_exit(void)
{
    proc_remove(counter_proc_entry);
    misc_deregister(&counter_device);
    pr_info("counter: module unloaded\n");
}

module_init(counter_init);
module_exit(counter_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tin");
MODULE_DESCRIPTION("Misc counter device driver");
