/*
 * sms_app.c - Sensor Monitoring System: hàm main, tham số, khởi tạo, shutdown
 *
 * Author : Nguyen Tran Quang - SE200988
 * Date   : 2026-10-16
 *
 * Kiến trúc:
 *   [Sensor]  --cb_push-->  shared_buffer  --cb_pop-->  [Processor]
 *   [Processor] --mq "/sms_alarm"--> [Alert]
 *   mọi thread --log_post: mq "/sms_log"--> [Logger] --> /tmp/sms.log
 *
 * SIGINT/SIGTERM bị chặn ở mọi thread; chỉ main nhận bằng sigtimedwait() rồi
 * tắt theo thứ tự: Sensor -> Processor -> Alert -> Logger (không thread nào
 * giữ 2 khóa cùng lúc -> không deadlock).
 *
 * Exit code: 0 = bình thường, 1 = tham số sai, 2 = không mở được thiết bị,
 *            3 = lỗi khởi tạo (bộ nhớ, mq, thread).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <sched.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "sms_common.h"
#include "circular_buffer.h"
#include "sensor_thread.h"
#include "processor_thread.h"
#include "alert_thread.h"
#include "logger_thread.h"

atomic_int g_running = 1;

/*
 * alarm_type_name - tên chuỗi của loại alarm
 * @type: AlarmType
 * Return: chuỗi hằng
 */
const char *alarm_type_name(int type)
{
    switch (type) {
    case ALARM_TEMP_HIGH:  return "TEMP_HIGH";
    case ALARM_HUMID_HIGH: return "HUMID_HIGH";
    case ALARM_TEMP_SPIKE: return "TEMP_SPIKE";
    default:               return "UNKNOWN";
    }
}

/*
 * now_wall_str - thời gian thực dạng "YYYY-mm-dd HH:MM:SS.mmm"
 * @buf: bộ đệm đích, @len: kích thước (>= 24)
 * Return: không có
 */
void now_wall_str(char *buf, int len)
{
    struct timespec ts;
    struct tm tm;
    char tmp[24];

    clock_gettime(CLOCK_REALTIME, &ts);
    localtime_r(&ts.tv_sec, &tm);
    strftime(tmp, sizeof(tmp), "%Y-%m-%d %H:%M:%S", &tm);
    snprintf(buf, len, "%s.%03ld", tmp, ts.tv_nsec / 1000000L);
}

/*
 * usage - in hướng dẫn sử dụng
 * @prog: argv[0]
 * Return: không có
 */
static void usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s [OPTIONS]\n"
        "  -i <ms>    Sensor sampling interval (default: 500ms)\n"
        "  -t <C>     Temperature alert threshold (default: 40.0)\n"
        "  -h <%%>     Humidity alert threshold (default: 80.0)\n"
        "  -d <sec>   Run duration in seconds, 0 = infinite (default: 60)\n"
        "  -v         Verbose mode: print each sample to stdout\n"
        "  -s <pol>   Sensor thread policy: fifo | other (default: fifo)\n"
        "  -D <path>  Device path, or 'sim' = built-in simulator (default: %s)\n"
        "  -L <path>  Log file path (default: %s)\n",
        prog, SMS_APP_DEFAULT_DEV, SMS_LOG_PATH);
}

/*
 * parse_double / parse_int - đổi chuỗi sang số, kiểm tra hợp lệ
 * Return: 0 nếu hợp lệ, -1 nếu sai
 */
static int parse_double(const char *s, double *out)
{
    char *end;

    errno = 0;
    *out = strtod(s, &end);
    return (errno || end == s || *end) ? -1 : 0;
}

static int parse_int(const char *s, int *out)
{
    char *end;
    long v;

    errno = 0;
    v = strtol(s, &end, 10);
    if (errno || end == s || *end || v < -2147483647L || v > 2147483647L)
        return -1;
    *out = (int)v;
    return 0;
}

/*
 * parse_args - đọc tham số dòng lệnh vào cfg
 * Return: 0 nếu hợp lệ, -1 nếu sai (đã in usage)
 */
static int parse_args(int argc, char *argv[], AppConfig *cfg)
{
    int opt;

    while ((opt = getopt(argc, argv, "i:t:h:d:vs:D:L:")) != -1) {
        switch (opt) {
        case 'i':
            if (parse_int(optarg, &cfg->interval_ms) || cfg->interval_ms < SMS_MIN_INTERVAL_MS ||
                cfg->interval_ms > SMS_MAX_INTERVAL_MS)
                goto bad;
            break;
        case 't':
            if (parse_double(optarg, &cfg->temp_th))
                goto bad;
            break;
        case 'h':
            if (parse_double(optarg, &cfg->hum_th))
                goto bad;
            break;
        case 'd':
            if (parse_int(optarg, &cfg->duration_s) || cfg->duration_s < 0)
                goto bad;
            break;
        case 'v':
            cfg->verbose = 1;
            break;
        case 's':
            if (strcmp(optarg, "fifo") == 0)
                cfg->use_fifo = 1;
            else if (strcmp(optarg, "other") == 0)
                cfg->use_fifo = 0;
            else
                goto bad;
            break;
        case 'D':
            cfg->dev_path = optarg;
            break;
        case 'L':
            cfg->log_path = optarg;
            break;
        default:
            usage(argv[0]);
            return -1;
        }
    }
    if (optind < argc)
        goto bad_extra;
    return 0;
bad:
    fprintf(stderr, "Invalid value for -%c: '%s'\n", opt, optarg);
    usage(argv[0]);
    return -1;
bad_extra:
    fprintf(stderr, "Unexpected argument: '%s'\n", argv[optind]);
    usage(argv[0]);
    return -1;
}

/*
 * start_thread - tạo thread với stack 256 KB (giảm bộ nhớ khóa bởi mlockall)
 * Return: 0 nếu thành công, mã lỗi pthread nếu thất bại
 */
static int start_thread(pthread_t *th, void *(*fn)(void *), void *arg)
{
    pthread_attr_t attr;
    int ret;

    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, THREAD_STACK_SIZE);
    ret = pthread_create(th, &attr, fn, arg);
    pthread_attr_destroy(&attr);
    return ret;
}

/*
 * main - khởi tạo, chạy 4 thread, chờ SIGINT/hết thời gian, shutdown sạch
 * Return: exit code (xem đầu file)
 */
int main(int argc, char *argv[])
{
    AppConfig cfg = {
        .interval_ms = 500, .temp_th = 40.0, .hum_th = 80.0, .duration_s = 60,
        .verbose = 0, .use_fifo = 1, .dev_path = SMS_APP_DEFAULT_DEV, .log_path = SMS_LOG_PATH,
    };
    CircularBuffer cb;
    SensorCtx sensor;
    ProcessorCtx proc;
    AlertCtx alert;
    LoggerCtx logger;
    pthread_t th_sensor, th_proc, th_alert, th_logger;
    struct mq_attr attr = { .mq_maxmsg = SMS_MQ_MAXMSG, .mq_msgsize = sizeof(AlarmMsg) };
    mqd_t alarm_q = (mqd_t)-1;
    struct timespec t_start, t_now;
    sigset_t sigs;
    const char *stop_reason = "duration reached";
    int fd = -1, ret = 0, mlock_ok = 0, jitter_cap, val;
    int started_logger = 0, started_alert = 0, started_proc = 0, started_sensor = 0;
    int *jitter = NULL;
    CbStats cbs;
    double uptime;
    char dur[24];

    if (parse_args(argc, argv, &cfg) != 0)
        return 1;
    /* stdout theo dòng kể cả khi chuyển hướng ra file -> log đúng thứ tự */
    setvbuf(stdout, NULL, _IOLBF, 0);

    memset(&sensor, 0, sizeof(sensor));
    memset(&proc, 0, sizeof(proc));
    memset(&alert, 0, sizeof(alert));
    memset(&logger, 0, sizeof(logger));
    memset(&cb, 0, sizeof(cb));

    /* 1) Chặn SIGINT/SIGTERM trước khi tạo thread -> mọi thread kế thừa.
     *    Đặt lại SIG_DFL trước: shell chạy nền (&) có thể để SIGINT ở SIG_IGN,
     *    khi đó signal bị bỏ luôn và sigtimedwait() không bao giờ nhận được. */
    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    sigemptyset(&sigs);
    sigaddset(&sigs, SIGINT);
    sigaddset(&sigs, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &sigs, NULL);

    /* 2) Khóa bộ nhớ để tránh page fault làm trễ Sensor Thread (chế độ RT) */
    if (cfg.use_fifo) {
        if (mlockall(MCL_CURRENT | MCL_FUTURE) == 0)
            mlock_ok = 1;
        else
            fprintf(stderr, "[main] WARN: mlockall failed: %s\n", strerror(errno));
    }

    /* 3) Cấp phát heap MỘT lần khi khởi động */
    jitter_cap = JITTER_MAX_SAMPLES;
    if (cfg.duration_s > 0) {
        long need = (long)cfg.duration_s * 1000L / cfg.interval_ms + 16;

        if (need < jitter_cap)
            jitter_cap = (int)need;
    }
    jitter = calloc((size_t)jitter_cap, sizeof(int));
    if (!jitter || cb_init(&cb, CB_CAPACITY) != 0) {
        fprintf(stderr, "[main] ERROR: out of memory\n");
        ret = 3;
        goto out;
    }

    /* 4) POSIX message queues */
    if (log_queue_open() != 0) {
        fprintf(stderr, "[main] ERROR: mq_open %s: %s\n", SMS_LOG_MQ_NAME, strerror(errno));
        ret = 3;
        goto out;
    }
    mq_unlink(SMS_ALARM_MQ_NAME);
    alarm_q = mq_open(SMS_ALARM_MQ_NAME, O_CREAT | O_EXCL | O_RDWR, 0600, &attr);
    if (alarm_q == (mqd_t)-1) {
        fprintf(stderr, "[main] ERROR: mq_open %s: %s\n", SMS_ALARM_MQ_NAME, strerror(errno));
        ret = 3;
        goto out;
    }

    /* 5) Mở driver và cấu hình interval bằng ioctl */
    if (strcmp(cfg.dev_path, "sim") == 0) {
        sensor.sim = 1;
        sensor.sim_seed = (unsigned int)getpid();
        printf("[main] device 'sim': built-in simulator, no driver (x86 host test)\n");
        goto start_threads;
    }
    fd = open(cfg.dev_path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        fprintf(stderr, "[main] ERROR: cannot open %s: %s (is sms_sensor_driver loaded?)\n",
                cfg.dev_path, strerror(errno));
        ret = 2;
        goto out;
    }
    val = cfg.interval_ms;
    if (ioctl(fd, SMS_SET_INTERVAL, &val) != 0)
        fprintf(stderr, "[main] WARN: ioctl SMS_SET_INTERVAL(%d) failed: %s\n",
                cfg.interval_ms, strerror(errno));
    else if (ioctl(fd, SMS_GET_INTERVAL, &val) == 0)
        printf("[main] driver interval set to %d ms (ioctl SMS_SET_INTERVAL)\n", val);

start_threads:
    /* 6) Tạo 4 thread: Logger -> Alert -> Processor -> Sensor */
    logger.path = cfg.log_path;
    alert.alarm_q = alarm_q;
    proc.cb = &cb;
    proc.alarm_q = alarm_q;
    proc.cfg = &cfg;
    sensor.fd = fd;
    sensor.cfg = &cfg;
    sensor.cb = &cb;
    sensor.jitter_us = jitter;
    sensor.jitter_cap = jitter_cap;

    if (start_thread(&th_logger, logger_thread_main, &logger) != 0)
        goto thread_fail;
    started_logger = 1;
    if (start_thread(&th_alert, alert_thread_main, &alert) != 0)
        goto thread_fail;
    started_alert = 1;
    if (start_thread(&th_proc, processor_thread_main, &proc) != 0)
        goto thread_fail;
    started_proc = 1;
    if (start_thread(&th_sensor, sensor_thread_main, &sensor) != 0)
        goto thread_fail;
    started_sensor = 1;

    log_post("[main] SMS started: pid=%d interval=%d ms T>%.1f H>%.1f duration=%d s policy=%s",
             (int)getpid(), cfg.interval_ms, cfg.temp_th, cfg.hum_th, cfg.duration_s,
             cfg.use_fifo ? "fifo" : "other");
    if (cfg.duration_s > 0)
        snprintf(dur, sizeof(dur), "%d s", cfg.duration_s);
    else
        snprintf(dur, sizeof(dur), "infinite");
    printf("[main] SMS running (pid %d): interval=%d ms, T>%.1f C, H>%.1f %%, duration=%s."
           " Ctrl+C to stop.\n", (int)getpid(), cfg.interval_ms, cfg.temp_th, cfg.hum_th, dur);
    fflush(stdout);

    /* 7) Chờ SIGINT/SIGTERM hoặc hết thời gian */
    clock_gettime(CLOCK_MONOTONIC, &t_start);
    for (;;) {
        struct timespec to = { 1, 0 };
        int sig;

        if (cfg.duration_s > 0) {
            clock_gettime(CLOCK_MONOTONIC, &t_now);
            if (t_now.tv_sec - t_start.tv_sec >= cfg.duration_s)
                break;
        }
        sig = sigtimedwait(&sigs, NULL, &to);
        if (sig == SIGINT || sig == SIGTERM) {
            stop_reason = (sig == SIGINT) ? "SIGINT" : "SIGTERM";
            break;
        }
    }
    goto shutdown;

thread_fail:
    fprintf(stderr, "[main] ERROR: pthread_create failed\n");
    ret = 3;

shutdown:
    /* 8) Graceful shutdown theo thứ tự producer -> consumer */
    printf("\n[main] %s -> shutting down...\n", ret ? "init error" : stop_reason);
    log_post("[main] shutdown requested (%s)", ret ? "init error" : stop_reason);
    atomic_store(&g_running, 0);
    if (started_sensor)
        pthread_join(th_sensor, NULL);         /* dừng sau <= 1 chu kỳ   */
    cb_shutdown(&cb);                          /* đánh thức Processor    */
    if (started_proc)
        pthread_join(th_proc, NULL);           /* xử lý nốt mẫu còn lại  */
    if (started_alert) {
        AlarmMsg stop = { .type = ALARM_SHUTDOWN };

        mq_send(alarm_q, (const char *)&stop, sizeof(stop), 0);
        pthread_join(th_alert, NULL);
    }
    clock_gettime(CLOCK_MONOTONIC, &t_now);
    uptime = (t_now.tv_sec - t_start.tv_sec) + (t_now.tv_nsec - t_start.tv_nsec) / 1e9;
    cb_get_stats(&cb, &cbs);
    log_post("[main] summary: samples=%lu processed=%lu alerts=%lu overflow=%lu uptime=%.1f s",
             sensor.reads_ok, proc.processed, alert.total, cbs.overflow_count, uptime);
    if (started_logger) {
        logger_send_shutdown();
        pthread_join(th_logger, NULL);         /* flush + đóng file log  */
    }
    if (fd >= 0) {
        close(fd);
        fd = -1;
        printf("[main] all threads joined, %s closed\n", cfg.dev_path);
    } else {
        printf("[main] all threads joined\n");
    }

    /* 9) Thông báo tổng kết */
    printf("================ SMS SUMMARY ================\n");
    printf("Stop reason        : %s\n", ret ? "init error" : stop_reason);
    printf("Uptime             : %.1f s\n", uptime);
    printf("Samples read       : %lu (EAGAIN retries %lu, EINTR %lu, EOF %lu, errors %lu)\n",
           sensor.reads_ok, sensor.eagain, sensor.eintr, sensor.eof, sensor.read_errors);
    printf("Samples processed  : %lu\n", proc.processed);
    printf("Shared buffer      : produced=%lu consumed=%lu overflow=%lu (%.2f%%) size=%d/%d\n",
           cbs.total_produced, cbs.total_consumed, cbs.overflow_count,
           (cbs.total_produced + cbs.overflow_count) ?
               cbs.overflow_count * 100.0 / (cbs.total_produced + cbs.overflow_count) : 0.0,
           cbs.current_size, cbs.capacity);
    printf("Alerts             : TEMP_HIGH=%lu HUMID_HIGH=%lu TEMP_SPIKE=%lu (total %lu)\n",
           alert.counts[ALARM_TEMP_HIGH], alert.counts[ALARM_HUMID_HIGH],
           alert.counts[ALARM_TEMP_SPIKE], alert.total);
    printf("Last avg10         : T=%.2f C  H=%.2f %%\n", proc.avg_t, proc.avg_h);
    printf("Log file           : %s (lines=%lu, flushes=%lu, rotations=%lu, dropped=%lu)\n",
           cfg.log_path, logger.lines, logger.flushes, logger.rotations, log_dropped_count());
    printf("Memory lock        : %s\n", cfg.use_fifo ? (mlock_ok ? "mlockall OK" : "mlockall FAILED") : "not used");
    printf("Sensor overruns    : %lu\n", sensor.overruns);
    sensor_print_jitter_report(&sensor, stdout);
    printf("=============================================\n");

out:
    if (fd >= 0)
        close(fd);
    if (alarm_q != (mqd_t)-1) {
        mq_close(alarm_q);
        mq_unlink(SMS_ALARM_MQ_NAME);
    }
    log_queue_close();
    if (cb.items)
        cb_destroy(&cb);
    free(jitter);
    if (mlock_ok)
        munlockall();
    return ret;
}
