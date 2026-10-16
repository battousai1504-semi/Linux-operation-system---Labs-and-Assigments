// SPDX-License-Identifier: GPL-2.0
/*
 * sms_sensor_driver.c - ASSIGNMENT-02: driver mô phỏng cảm biến nhiệt độ/độ ẩm
 *
 * Author: Nguyen Tran Quang - SE200988
 * Date  : 2026-10-16
 *
 *   /dev/sms_sensor (misc device, major 10)
 *     read  -> 1 dòng CSV "<timestamp_ms>,<temperature>,<humidity>\n"
 *              Hai lần đọc phải cách nhau ít nhất interval_ms (cho phép sớm
 *              hơn tối đa 10% để bù jitter của thread userspace):
 *                - fd blocking    : driver ngủ cho đủ khoảng cách rồi trả mẫu
 *                - fd O_NONBLOCK  : trả -EAGAIN (chưa sẵn sàng)
 *     ioctl -> SMS_SET_INTERVAL / SMS_GET_INTERVAL (ms, mặc định 100)
 *   /proc/sms_stats -> read_count, last_temperature, last_humidity, uptime...
 *
 * Giá trị mô phỏng: random walk bằng get_random_u32(), giữ trong
 * 15.0-45.0 °C và 30.0-90.0 %. Với xác suất spike_rate % mỗi mẫu là một
 * "bất thường" (nhiệt độ 40.5-45.0 hoặc độ ẩm 80.5-90.0) để app có alert.
 * spike_rate đổi được lúc chạy:
 *   echo 30 > /sys/module/sms_sensor_driver/parameters/spike_rate
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/random.h>     /* get_random_u32 */
#include <linux/ktime.h>
#include <linux/delay.h>      /* msleep_interruptible */
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/atomic.h>
#include "sms_sensor.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nguyen Tran Quang - SE200988");
MODULE_DESCRIPTION("ASSIGNMENT-02: simulated temperature/humidity sensor (/dev/sms_sensor)");
MODULE_VERSION("1.0");

static int spike_rate = 5;   /* % số mẫu là bất thường */
module_param(spike_rate, int, 0644);
MODULE_PARM_DESC(spike_rate, "Percent of samples that are anomalies (0-100, default 5)");

/* ======================== Trạng thái driver ======================== */
struct sms_state {
    struct mutex lock;               /* bảo vệ mọi trường bên dưới  */
    struct sms_raw_sample last;      /* mẫu trả ra gần nhất          */
    s32 base_temp_x10;               /* giá trị nền của random walk   */
    s32 base_humid_x10;
    u64 read_count;                  /* số lần read trả mẫu thành công */
    u64 eagain_count;                /* số lần trả -EAGAIN            */
    u64 anomaly_count;               /* số mẫu bất thường đã sinh     */
    u64 last_read_ns;                /* thời điểm read thành công trước */
    bool has_read;
    int interval_ms;
    u64 load_ns;                     /* thời điểm insmod (tính uptime) */
};

static struct sms_state sms;
static atomic_t open_count = ATOMIC_INIT(0);
static struct proc_dir_entry *proc_entry;

/*
 * sms_ktime_get_ms - thời gian hiện tại (ms, monotonic từ lúc boot)
 * Kernel 5.15 không có hàm ktime_get_ms() nên viết bằng ktime_to_ms(ktime_get()).
 * Return: số ms
 */
static inline u64 sms_ktime_get_ms(void)
{
    return ktime_to_ms(ktime_get());
}

/*
 * sms_rand_range - số nguyên ngẫu nhiên trong [lo, hi] dùng get_random_u32()
 * @lo, @hi: cận dưới, cận trên (lo <= hi)
 * Return: giá trị ngẫu nhiên
 */
static s32 sms_rand_range(s32 lo, s32 hi)
{
    return lo + (s32)(get_random_u32() % (u32)(hi - lo + 1));
}

/*
 * sms_clamp - kẹp v vào [lo, hi]
 * Return: giá trị sau khi kẹp
 */
static s32 sms_clamp(s32 v, s32 lo, s32 hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/*
 * sms_generate_sample - sinh một mẫu mới vào sms.last. Gọi khi đang giữ sms.lock.
 * @now_ms: timestamp của mẫu
 * Return: không có
 */
static void sms_generate_sample(u64 now_ms)
{
    int rate = sms_clamp(READ_ONCE(spike_rate), 0, 100);
    s32 t, h;

    /* random walk: nhiệt độ ±0.5 °C, độ ẩm ±1.0 % mỗi mẫu */
    sms.base_temp_x10 = sms_clamp(sms.base_temp_x10 + sms_rand_range(-5, 5),
                                  SMS_TEMP_MIN_X10, SMS_TEMP_MAX_X10);
    sms.base_humid_x10 = sms_clamp(sms.base_humid_x10 + sms_rand_range(-10, 10),
                                   SMS_HUMID_MIN_X10, SMS_HUMID_MAX_X10);
    t = sms.base_temp_x10;
    h = sms.base_humid_x10;

    /* bất thường một mẫu (không đổi giá trị nền) */
    if (sms_rand_range(1, 100) <= rate) {
        if (sms_rand_range(0, 1) == 0)
            t = sms_rand_range(405, SMS_TEMP_MAX_X10);
        else
            h = sms_rand_range(805, SMS_HUMID_MAX_X10);
        sms.anomaly_count++;
    }

    sms.last.timestamp_ms = now_ms;
    sms.last.temp_x10 = t;
    sms.last.humid_x10 = h;
}

/* ======================== file_operations ======================== */

/*
 * sms_open - đếm số lần mở /dev/sms_sensor
 * @inode, @file: của thiết bị
 * Return: 0
 */
static int sms_open(struct inode *inode, struct file *file)
{
    atomic_inc(&open_count);
    return 0;
}

/*
 * sms_release - gọi khi đóng fd cuối cùng
 * @inode, @file: của thiết bị
 * Return: 0
 */
static int sms_release(struct inode *inode, struct file *file)
{
    return 0;
}

/*
 * sms_read - trả một dòng CSV "<timestamp_ms>,<temp>,<humid>\n"
 * @file:  file đang đọc (kiểm tra O_NONBLOCK)
 * @ubuf:  buffer userspace, phải chứa được cả dòng (>= SMS_LINE_MAX là chắc chắn)
 * @count: kích thước buffer userspace
 * @ppos:  không dùng - mỗi read là một mẫu mới (giống luồng dữ liệu cảm biến)
 * Return: số byte của dòng; -EAGAIN nếu O_NONBLOCK và chưa đủ interval;
 *         -EINTR nếu bị signal khi đang chờ; -EINVAL nếu buffer quá nhỏ;
 *         -EFAULT nếu copy lỗi
 */
static ssize_t sms_read(struct file *file, char __user *ubuf,
                        size_t count, loff_t *ppos)
{
    char line[SMS_LINE_MAX];
    u64 now_ns, min_gap_ns, elapsed;
    int len;

    if (mutex_lock_interruptible(&sms.lock))
        return -EINTR;

    for (;;) {
        now_ns = ktime_get_ns();
        /* khoảng tối thiểu = interval - 10% (dung sai cho jitter) */
        min_gap_ns = (u64)sms.interval_ms * NSEC_PER_MSEC;
        min_gap_ns -= div_u64(min_gap_ns, 10);
        elapsed = now_ns - sms.last_read_ns;
        if (!sms.has_read || elapsed >= min_gap_ns)
            break;                              /* đã đủ khoảng cách */

        if (file->f_flags & O_NONBLOCK) {
            sms.eagain_count++;
            mutex_unlock(&sms.lock);
            return -EAGAIN;                     /* chưa sẵn sàng */
        }
        mutex_unlock(&sms.lock);
        /* ngủ phần còn thiếu; bị signal -> -EINTR */
        /* div_u64: ARM 32-bit không chia trực tiếp số 64-bit trong kernel */
        if (msleep_interruptible(div_u64(min_gap_ns - elapsed + NSEC_PER_MSEC - 1,
                                         NSEC_PER_MSEC)))
            return -EINTR;
        if (mutex_lock_interruptible(&sms.lock))
            return -EINTR;
    }

    sms_generate_sample(sms_ktime_get_ms());
    sms.last_read_ns = now_ns;
    sms.has_read = true;
    sms.read_count++;

    len = scnprintf(line, sizeof(line), "%llu,%d.%d,%d.%d\n",
                    sms.last.timestamp_ms,
                    sms.last.temp_x10 / 10, sms.last.temp_x10 % 10,
                    sms.last.humid_x10 / 10, sms.last.humid_x10 % 10);
    mutex_unlock(&sms.lock);

    if (count < (size_t)len)
        return -EINVAL;
    if (copy_to_user(ubuf, line, len))
        return -EFAULT;
    return len;
}

/*
 * sms_ioctl - SMS_SET_INTERVAL / SMS_GET_INTERVAL
 * @file: file đang mở
 * @cmd:  mã lệnh
 * @arg:  địa chỉ userspace của một int (ms)
 * Return: 0; -EINVAL nếu interval ngoài [1, 60000]; -EFAULT; -ENOTTY nếu sai lệnh
 */
static long sms_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int __user *uarg = (int __user *)arg;
    int ms;

    switch (cmd) {
    case SMS_SET_INTERVAL:
        if (copy_from_user(&ms, uarg, sizeof(ms)))
            return -EFAULT;
        if (ms < SMS_MIN_INTERVAL_MS || ms > SMS_MAX_INTERVAL_MS)
            return -EINVAL;
        mutex_lock(&sms.lock);
        sms.interval_ms = ms;
        mutex_unlock(&sms.lock);
        pr_info("sms_sensor: ioctl SET_INTERVAL -> %d ms\n", ms);
        return 0;

    case SMS_GET_INTERVAL:
        mutex_lock(&sms.lock);
        ms = sms.interval_ms;
        mutex_unlock(&sms.lock);
        if (copy_to_user(uarg, &ms, sizeof(ms)))
            return -EFAULT;
        return 0;

    default:
        return -ENOTTY;
    }
}

static const struct file_operations sms_fops = {
    .owner          = THIS_MODULE,
    .open           = sms_open,
    .release        = sms_release,
    .read           = sms_read,
    .unlocked_ioctl = sms_ioctl,
    .llseek         = noop_llseek,
};

static struct miscdevice sms_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = SMS_DEVICE_NAME,
    .fops  = &sms_fops,
    .mode  = 0666,
};

/* ============================ procfs ============================ */

/*
 * sms_proc_show - nội dung /proc/sms_stats
 * @m: seq_file đích
 * @v: không dùng
 * Return: 0
 */
static int sms_proc_show(struct seq_file *m, void *v)
{
    u64 uptime_s = div_u64(ktime_get_ns() - sms.load_ns, NSEC_PER_SEC);

    mutex_lock(&sms.lock);
    seq_puts(m, "SMS Sensor Driver Statistics\n");
    seq_puts(m, "============================\n");
    seq_printf(m, "read_count       : %llu\n", sms.read_count);
    seq_printf(m, "last_temperature : %d.%d C\n",
               sms.last.temp_x10 / 10, sms.last.temp_x10 % 10);
    seq_printf(m, "last_humidity    : %d.%d %%\n",
               sms.last.humid_x10 / 10, sms.last.humid_x10 % 10);
    seq_printf(m, "last_timestamp   : %llu ms\n", sms.last.timestamp_ms);
    seq_printf(m, "uptime           : %llu s\n", uptime_s);
    seq_printf(m, "interval_ms      : %d\n", sms.interval_ms);
    seq_printf(m, "eagain_count     : %llu\n", sms.eagain_count);
    seq_printf(m, "anomaly_count    : %llu\n", sms.anomaly_count);
    seq_printf(m, "spike_rate       : %d %%\n", READ_ONCE(spike_rate));
    seq_printf(m, "open_count       : %d\n", atomic_read(&open_count));
    mutex_unlock(&sms.lock);
    return 0;
}

/*
 * sms_proc_open - gắn sms_proc_show vào file /proc vừa mở
 * Return: kết quả single_open()
 */
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

/* ========================== init / exit ========================== */

/*
 * sms_init - insmod: khởi tạo trạng thái, đăng ký misc device và /proc/sms_stats
 * Return: 0 nếu thành công, mã lỗi âm nếu thất bại
 */
static int __init sms_init(void)
{
    int ret;

    mutex_init(&sms.lock);
    sms.base_temp_x10 = 280;       /* 28.0 °C */
    sms.base_humid_x10 = 600;      /* 60.0 %  */
    sms.last.temp_x10 = sms.base_temp_x10;
    sms.last.humid_x10 = sms.base_humid_x10;
    sms.interval_ms = SMS_DEFAULT_INTERVAL_MS;
    sms.load_ns = ktime_get_ns();

    ret = misc_register(&sms_misc);
    if (ret) {
        pr_err("sms_sensor: misc_register failed: %d\n", ret);
        return ret;
    }

    proc_entry = proc_create(SMS_PROC_NAME, 0444, NULL, &sms_proc_ops);
    if (!proc_entry) {
        misc_deregister(&sms_misc);
        pr_err("sms_sensor: cannot create /proc/%s\n", SMS_PROC_NAME);
        return -ENOMEM;
    }

    pr_info("sms_sensor: loaded, /dev/%s (major 10, minor %d), interval=%d ms, spike_rate=%d%%\n",
            SMS_DEVICE_NAME, sms_misc.minor, sms.interval_ms, spike_rate);
    return 0;
}

/*
 * sms_exit - rmmod: gỡ /proc rồi gỡ misc device
 * Return: không có
 */
static void __exit sms_exit(void)
{
    proc_remove(proc_entry);
    misc_deregister(&sms_misc);
    pr_info("sms_sensor: unloaded (read_count=%llu)\n", sms.read_count);
}

module_init(sms_init);
module_exit(sms_exit);
