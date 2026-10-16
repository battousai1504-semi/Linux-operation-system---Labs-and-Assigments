/*
 * processor_thread.c - Processor Thread: lấy mẫu từ shared_buffer, tính rolling
 *                      average 10 mẫu, phát alarm vào alarm_queue
 *
 * Author : Nguyen Tran Quang - SE200988
 * Date   : 2026-10-16
 *
 * Điều kiện alarm (so với giá trị mẫu thô):
 *   TEMP_HIGH  : temperature > cfg->temp_th   (mặc định 40.0 °C)
 *   HUMID_HIGH : humidity    > cfg->hum_th    (mặc định 80.0 %)
 *   TEMP_SPIKE : |temperature - mẫu trước| > 5.0 °C
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <pthread.h>
#include "processor_thread.h"
#include "logger_thread.h"

/*
 * send_alarm - gửi 1 alarm vào mq (chờ tối đa 200 ms nếu queue đầy)
 * @p: processor, @type/@seq/@ts_ms/@value/@threshold: nội dung alarm
 * Return: 0 nếu gửi được, -1 nếu bỏ
 */
static int send_alarm(ProcessorCtx *p, int type, unsigned int seq, long long ts_ms,
                      double value, double threshold)
{
    AlarmMsg m = { .type = type, .seq = seq, .ts_ms = ts_ms,
                   .value = value, .threshold = threshold };
    struct timespec deadline;

    clock_gettime(CLOCK_REALTIME, &deadline);
    ts_add_ms(&deadline, 200);
    if (mq_timedsend(p->alarm_q, (const char *)&m, sizeof(m), 0, &deadline) == 0) {
        p->alarms_sent++;
        return 0;
    }
    p->alarms_dropped++;
    log_post("[processor] WARN: alarm dropped (%s)", strerror(errno));
    return -1;
}

/*
 * processor_check_alarms - kiểm tra ngưỡng và gửi alarm
 * (noinline để đặt breakpoint GDB và "set variable temperature=45.0")
 * @p: processor, @seq/@ts_ms: thông tin mẫu
 * @temperature, @humidity: giá trị mẫu
 * Return: số alarm đã phát cho mẫu này
 */
__attribute__((noinline))
int processor_check_alarms(ProcessorCtx *p, unsigned int seq, long long ts_ms,
                           double temperature, double humidity)
{
    int fired = 0;

    if (temperature > p->cfg->temp_th) {
        send_alarm(p, ALARM_TEMP_HIGH, seq, ts_ms, temperature, p->cfg->temp_th);
        fired++;
    }
    if (humidity > p->cfg->hum_th) {
        send_alarm(p, ALARM_HUMID_HIGH, seq, ts_ms, humidity, p->cfg->hum_th);
        fired++;
    }
    if (p->has_prev && fabs(temperature - p->prev_temp) > TEMP_SPIKE_DELTA) {
        send_alarm(p, ALARM_TEMP_SPIKE, seq, ts_ms,
                   fabs(temperature - p->prev_temp), TEMP_SPIKE_DELTA);
        fired++;
    }
    p->prev_temp = temperature;
    p->has_prev = 1;
    return fired;
}

/*
 * update_average - thêm mẫu vào cửa sổ 10 mẫu và tính lại trung bình
 * @p: processor, @d: mẫu mới
 * Return: không có
 */
static void update_average(ProcessorCtx *p, const SensorData *d)
{
    double st = 0, sh = 0;
    int i;

    p->win_t[p->win_pos] = d->temperature;
    p->win_h[p->win_pos] = d->humidity;
    p->win_pos = (p->win_pos + 1) % ROLL_WINDOW;
    if (p->win_count < ROLL_WINDOW)
        p->win_count++;
    for (i = 0; i < p->win_count; i++) {
        st += p->win_t[i];
        sh += p->win_h[i];
    }
    p->avg_t = st / p->win_count;
    p->avg_h = sh / p->win_count;
}

/*
 * processor_thread_main - vòng lặp của Processor Thread
 * @arg: ProcessorCtx*
 * Return: NULL
 */
void *processor_thread_main(void *arg)
{
    ProcessorCtx *p = arg;
    SensorData d;

    pthread_setname_np(pthread_self(), "sms_processor");
    log_post("[processor] thread started (window=%d, T>%.1f, H>%.1f, spike>%.1f)",
             ROLL_WINDOW, p->cfg->temp_th, p->cfg->hum_th, TEMP_SPIKE_DELTA);

    while (cb_pop(p->cb, &d) == 0) {          /* chờ trên cond khi rỗng */
        update_average(p, &d);
        processor_check_alarms(p, d.seq, d.ts_ms, d.temperature, d.humidity);
        p->processed++;

        if (p->cfg->verbose)
            printf("[SAMPLE #%4u] ts=%lld ms  T=%.1f C  H=%.1f %%  | avg10 T=%.2f C  H=%.2f %%\n",
                   d.seq, d.ts_ms, d.temperature, d.humidity, p->avg_t, p->avg_h);
        log_post("[processor] sample=#%u ts=%lld T=%.1f H=%.1f avgT=%.2f avgH=%.2f",
                 d.seq, d.ts_ms, d.temperature, d.humidity, p->avg_t, p->avg_h);
    }

    log_post("[processor] thread stopped (processed=%lu, alarms=%lu, dropped=%lu)",
             p->processed, p->alarms_sent, p->alarms_dropped);
    return NULL;
}
