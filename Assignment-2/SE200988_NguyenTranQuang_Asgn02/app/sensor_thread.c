/*
 * sensor_thread.c - Sensor Thread: đọc cảm biến đúng chu kỳ và đo jitter
 *
 * Author : Nguyen Tran Quang - SE200988
 * Date   : 2026-10-16
 *
 * - sched_setscheduler(SCHED_FIFO, prio 50) nếu chạy với -s fifo
 * - Chu kỳ tuyệt đối: clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME)
 * - jitter = thời điểm thức dậy thực tế - thời điểm dự kiến (µs)
 * - fd mở O_NONBLOCK: driver trả EAGAIN nếu chưa đủ interval -> thử lại sau 1 ms
 * - "-D sim" (mặc định của bản x86): không có driver, sinh dòng CSV cùng định
 *   dạng ngay trong thread này -> chạy được Valgrind/strace/perf/GDB trên host
 * Các bộ đếm trong SensorCtx chỉ thread này ghi; main đọc sau pthread_join.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sched.h>
#include <pthread.h>
#include "sensor_thread.h"
#include "logger_thread.h"

/*
 * sensor_set_rt - chuyển thread hiện tại sang SCHED_FIFO
 * @s: sensor (ghi lại policy/prio thực tế)
 * Return: 0 nếu thành công, -1 nếu thất bại (vẫn chạy SCHED_OTHER)
 */
static int sensor_set_rt(SensorCtx *s)
{
    struct sched_param sp = { .sched_priority = SENSOR_RT_PRIO };
    int ret = 0;

    /* Trên Linux, pid 0 = thread đang gọi (không phải cả process) */
    if (sched_setscheduler(0, SCHED_FIFO, &sp) != 0) {
        fprintf(stderr, "[sensor] WARN: sched_setscheduler(SCHED_FIFO, %d) failed: %s"
                " -> keep SCHED_OTHER\n", SENSOR_RT_PRIO, strerror(errno));
        ret = -1;
    }
    s->policy = sched_getscheduler(0);
    sched_getparam(0, &sp);
    s->prio = sp.sched_priority;
    return ret;
}

/*
 * sim_rand_range - số ngẫu nhiên trong [lo, hi] cho bộ mô phỏng
 * Return: giá trị ngẫu nhiên
 */
static int sim_rand_range(SensorCtx *s, int lo, int hi)
{
    return lo + (int)(rand_r(&s->sim_seed) % (unsigned int)(hi - lo + 1));
}

/*
 * sim_clamp - kẹp v vào [lo, hi]
 * Return: giá trị sau khi kẹp
 */
static int sim_clamp(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/*
 * sensor_sim_line - bộ mô phỏng: tạo 1 dòng CSV giống hệt driver
 * (random walk + 5% mẫu bất thường), dùng khi chạy trên x86 không có driver
 * @s: sensor, @line/@len: bộ đệm đích
 * Return: số ký tự đã ghi
 */
static int sensor_sim_line(SensorCtx *s, char *line, size_t len)
{
    struct timespec now;
    int t, h;

    if (s->sim_t_x10 == 0) {                   /* lần đầu: 28.0 C, 60.0 % */
        s->sim_t_x10 = 280;
        s->sim_h_x10 = 600;
    }
    s->sim_t_x10 = sim_clamp(s->sim_t_x10 + sim_rand_range(s, -5, 5),
                             SMS_TEMP_MIN_X10, SMS_TEMP_MAX_X10);
    s->sim_h_x10 = sim_clamp(s->sim_h_x10 + sim_rand_range(s, -10, 10),
                             SMS_HUMID_MIN_X10, SMS_HUMID_MAX_X10);
    t = s->sim_t_x10;
    h = s->sim_h_x10;
    if (sim_rand_range(s, 1, 100) <= 5) {
        if (sim_rand_range(s, 0, 1) == 0)
            t = sim_rand_range(s, 405, SMS_TEMP_MAX_X10);
        else
            h = sim_rand_range(s, 805, SMS_HUMID_MAX_X10);
    }
    clock_gettime(CLOCK_MONOTONIC, &now);
    return snprintf(line, len, "%lld,%d.%d,%d.%d\n",
                    (long long)now.tv_sec * 1000LL + now.tv_nsec / 1000000L,
                    t / 10, t % 10, h / 10, h % 10);
}

/*
 * sensor_read_sample - đọc và parse 1 dòng CSV, xử lý EAGAIN/EINTR/EOF
 * @s: sensor, @d: nơi nhận mẫu
 * Return: 0 nếu có mẫu hợp lệ, -1 nếu bỏ qua chu kỳ này
 */
static int sensor_read_sample(SensorCtx *s, SensorData *d)
{
    char line[SMS_LINE_MAX + 1];
    int attempt;

    for (attempt = 0; attempt <= READ_RETRY_MAX; attempt++) {
        ssize_t n;

        if (s->sim)
            n = sensor_sim_line(s, line, sizeof(line));
        else
            n = read(s->fd, line, SMS_LINE_MAX);

        if (n > 0) {
            line[n] = '\0';
            if (sscanf(line, "%lld,%lf,%lf", &d->ts_ms, &d->temperature, &d->humidity) != 3) {
                s->parse_errors++;
                log_post("[sensor] WARN: bad line from driver: %.40s", line);
                return -1;
            }
            s->reads_ok++;
            return 0;
        }
        if (n == 0) {                          /* EOF: driver không có dữ liệu */
            s->eof++;
            log_post("[sensor] WARN: EOF from driver, skip this period");
            return -1;
        }
        if (errno == EAGAIN) {                 /* driver chưa sẵn sàng -> chờ 1 ms */
            struct timespec ms1 = { 0, 1000000L };

            s->eagain++;
            nanosleep(&ms1, NULL);
            continue;
        }
        if (errno == EINTR) {                  /* bị signal ngắt -> thử lại */
            s->eintr++;
            continue;
        }
        s->read_errors++;
        log_post("[sensor] ERROR: read %s: %s", s->cfg->dev_path, strerror(errno));
        return -1;
    }
    log_post("[sensor] WARN: driver not ready after %d retries, skip", READ_RETRY_MAX);
    return -1;
}

/*
 * sensor_thread_main - vòng lặp chu kỳ của Sensor Thread
 * @arg: SensorCtx*
 * Return: NULL
 */
void *sensor_thread_main(void *arg)
{
    SensorCtx *s = arg;
    struct timespec next, now;
    SensorData d;
    long period_us = s->cfg->interval_ms * 1000L;

    pthread_setname_np(pthread_self(), "sms_sensor");
    if (s->cfg->use_fifo)
        sensor_set_rt(s);
    else {
        s->policy = sched_getscheduler(0);
        s->prio = 0;
    }
    log_post("[sensor] thread started (%s prio %d, interval %d ms, device %s)",
             s->policy == SCHED_FIFO ? "SCHED_FIFO" : "SCHED_OTHER", s->prio,
             s->cfg->interval_ms, s->cfg->dev_path);

    clock_gettime(CLOCK_MONOTONIC, &next);
    while (atomic_load(&g_running)) {
        long jitter_us;

        ts_add_ms(&next, s->cfg->interval_ms);
        while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL) == EINTR)
            ;
        clock_gettime(CLOCK_MONOTONIC, &now);
        jitter_us = timespec_diff_us(&now, &next);
        if (jitter_us < 0)
            jitter_us = 0;
        if (s->jitter_count < s->jitter_cap)
            s->jitter_us[s->jitter_count++] = (int)jitter_us;

        if (!atomic_load(&g_running))
            break;
        if (jitter_us > period_us) {           /* trễ cả 1 chu kỳ: đồng bộ lại */
            s->overruns++;
            next = now;
        }

        if (sensor_read_sample(s, &d) != 0)
            continue;
        d.seq = ++s->seq;
        if (cb_push(s->cb, &d) != 0) {
            s->dropped++;
            log_post("[sensor] WARN: shared_buffer full, drop sample #%u", d.seq);
        }
    }

    log_post("[sensor] thread stopped (reads=%lu, eagain=%lu, dropped=%lu)",
             s->reads_ok, s->eagain, s->dropped);
    return NULL;
}

/* So sánh 2 int cho qsort */
static int cmp_int(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;

    return (x > y) - (x < y);
}

/*
 * sensor_print_jitter_report - in Min/Avg/Max/P99 và histogram text
 * (gọi sau khi Sensor Thread đã join; sắp xếp mảng jitter tại chỗ)
 * @s: sensor, @out: stdout hoặc file
 * Return: không có
 */
void sensor_print_jitter_report(SensorCtx *s, FILE *out)
{
    int n = s->jitter_count, i, p99_idx;
    long long sum = 0;
    int bins[4] = { 0, 0, 0, 0 };
    const char *labels[4] = { "0-100us", "100-500us", "500-1000us", ">1000us" };

    fprintf(out, "===== JITTER REPORT (Sensor Thread) =====\n");
    fprintf(out, "Policy          : %s (prio %d)\n",
            s->policy == SCHED_FIFO ? "SCHED_FIFO" : "SCHED_OTHER", s->prio);
    fprintf(out, "Interval        : %d ms\n", s->cfg->interval_ms);
    fprintf(out, "Samples         : %d\n", n);
    if (n == 0) {
        fprintf(out, "(no samples)\n");
        return;
    }
    for (i = 0; i < n; i++) {
        int j = s->jitter_us[i];

        sum += j;
        bins[j < 100 ? 0 : j < 500 ? 1 : j < 1000 ? 2 : 3]++;
    }
    qsort(s->jitter_us, n, sizeof(int), cmp_int);
    p99_idx = (int)((n * 99LL + 99) / 100) - 1;    /* nearest-rank */
    if (p99_idx < 0)
        p99_idx = 0;
    fprintf(out, "Min / Avg / Max : %d / %.1f / %d us\n",
            s->jitter_us[0], (double)sum / n, s->jitter_us[n - 1]);
    fprintf(out, "P99             : %d us\n", s->jitter_us[p99_idx]);
    fprintf(out, "Histogram:\n");
    for (i = 0; i < 4; i++) {
        int bar = (int)(bins[i] * 50LL / n), k;

        fprintf(out, "  %-11s: %6d (%5.1f%%) ", labels[i], bins[i], bins[i] * 100.0 / n);
        for (k = 0; k < bar; k++)
            fputc('#', out);
        fputc('\n', out);
    }
}
