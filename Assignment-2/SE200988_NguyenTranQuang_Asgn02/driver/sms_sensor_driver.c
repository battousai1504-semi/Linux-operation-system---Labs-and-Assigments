/* sms_sensor_driver.c - Cam bien nhiet do / do am gia lap (ASSIGNMENT-02)
 * Author : Nguyen Tran Quang - SE200988
 * Date   : 2026-10-16 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/random.h>
#include <linux/ktime.h>
#include <linux/math64.h>

#include "sms_sensor.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nguyen Tran Quang - SE200988");
MODULE_DESCRIPTION("ASSIGNMENT-02 simulated temperature/humidity sensor");

#define DEVICE_NAME "sms_sensor"
#define PROC_NAME   "sms_stats"

static int major;
static struct class *sms_class;
static struct proc_dir_entry *proc_entry;
static DEFINE_MUTEX(sms_lock);

static int temp_x10  = 300;
static int humid_x10 = 600;
static unsigned long read_count;
static int interval_ms = 100;
static s64 load_time_ms;
static s64 last_read_ms;

static s64 now_ms(void)
{
	return ktime_to_ms(ktime_get());
}

static int rand_range(int min, int max)
{
	return min + (int)(get_random_u32() % (u32)(max - min + 1));
}

static void make_new_sample(void)
{
	if (rand_range(1, 20) == 1)
		temp_x10 += (rand_range(0, 1) ? 60 : -60);
	else
		temp_x10 += rand_range(-10, 10);

	humid_x10 += rand_range(-20, 20);

	temp_x10  = clamp(temp_x10, 150, 450);
	humid_x10 = clamp(humid_x10, 300, 900);
}

static int sms_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int sms_release(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t sms_read(struct file *file, char __user *buf,
			size_t count, loff_t *ppos)
{
	char line[64];
	int len;
	s64 now;

	if (*ppos > 0)
		return 0;

	mutex_lock(&sms_lock);
	now = now_ms();

	if (now - last_read_ms < interval_ms * 9 / 10) {
		mutex_unlock(&sms_lock);
		return -EAGAIN;
	}

	last_read_ms = now;
	make_new_sample();
	read_count++;
	len = scnprintf(line, sizeof(line), "%lld,%d.%d,%d.%d\n", now,
			temp_x10 / 10, temp_x10 % 10,
			humid_x10 / 10, humid_x10 % 10);
	mutex_unlock(&sms_lock);

	if ((size_t)len > count)
		len = count;
	if (copy_to_user(buf, line, len))
		return -EFAULT;

	*ppos += len;
	return len;
}

static long sms_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int value;

	if (cmd != SMS_SET_INTERVAL)
		return -ENOTTY;

	if (copy_from_user(&value, (int __user *)arg, sizeof(value)))
		return -EFAULT;

	if (value < 1 || value > 60000)
		return -EINVAL;

	mutex_lock(&sms_lock);
	interval_ms = value;
	mutex_unlock(&sms_lock);

	pr_info("sms_sensor: interval set to %d ms\n", value);
	return 0;
}

static const struct file_operations sms_fops = {
	.owner          = THIS_MODULE,
	.open           = sms_open,
	.release        = sms_release,
	.read           = sms_read,
	.unlocked_ioctl = sms_ioctl,
};

static int sms_proc_show(struct seq_file *m, void *v)
{
	mutex_lock(&sms_lock);
	seq_printf(m, "read_count       : %lu\n", read_count);
	seq_printf(m, "last_temperature : %d.%d C\n", temp_x10 / 10, temp_x10 % 10);
	seq_printf(m, "last_humidity    : %d.%d %%\n", humid_x10 / 10, humid_x10 % 10);
	seq_printf(m, "interval_ms      : %d\n", interval_ms);
	seq_printf(m, "uptime_s         : %lld\n",
		   div_s64(now_ms() - load_time_ms, 1000));
	mutex_unlock(&sms_lock);
	return 0;
}

static int sms_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, sms_proc_show, NULL);
}

static const struct proc_ops sms_proc_ops = {
	.proc_open    = sms_proc_open,
	.proc_read    = seq_read,
	.proc_lseek   = seq_lseek,
	.proc_release = single_release,
};

static int __init sms_init(void)
{
	struct device *dev;

	major = register_chrdev(0, DEVICE_NAME, &sms_fops);
	if (major < 0) {
		pr_err("sms_sensor: register_chrdev failed\n");
		return major;
	}

	sms_class = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(sms_class)) {
		unregister_chrdev(major, DEVICE_NAME);
		return PTR_ERR(sms_class);
	}

	dev = device_create(sms_class, NULL, MKDEV(major, 0), NULL, DEVICE_NAME);
	if (IS_ERR(dev)) {
		class_destroy(sms_class);
		unregister_chrdev(major, DEVICE_NAME);
		return PTR_ERR(dev);
	}

	proc_entry = proc_create(PROC_NAME, 0444, NULL, &sms_proc_ops);
	if (!proc_entry) {
		device_destroy(sms_class, MKDEV(major, 0));
		class_destroy(sms_class);
		unregister_chrdev(major, DEVICE_NAME);
		return -ENOMEM;
	}

	load_time_ms = now_ms();
	pr_info("sms_sensor: loaded, major=%d, /dev/%s, /proc/%s\n",
		major, DEVICE_NAME, PROC_NAME);
	return 0;
}

static void __exit sms_exit(void)
{
	proc_remove(proc_entry);
	device_destroy(sms_class, MKDEV(major, 0));
	class_destroy(sms_class);
	unregister_chrdev(major, DEVICE_NAME);
	pr_info("sms_sensor: unloaded, total reads=%lu\n", read_count);
}

module_init(sms_init);
module_exit(sms_exit);
