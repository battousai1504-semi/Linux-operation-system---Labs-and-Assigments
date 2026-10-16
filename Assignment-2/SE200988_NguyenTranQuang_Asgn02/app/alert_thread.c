/*
 * alert_thread.c - Alert Thread: chờ trên alarm_queue, in cảnh báo, đếm theo loại
 *
 * Author : Nguyen Tran Quang - SE200988
 * Date   : 2026-10-16
 *
 * Thread block trong mq_receive() và chỉ thức dậy khi Processor gửi alarm
 * (hoặc main gửi ALARM_SHUTDOWN lúc tắt). counts[] chỉ thread này ghi;
 * main chỉ đọc sau pthread_join -> không cần khóa.
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include "alert_thread.h"
#include "logger_thread.h"

/*
 * alert_handle - xử lý 1 alarm: đếm, in ra stderr, gửi sang Logger
 * @ctx: trạng thái Alert Thread, @msg: alarm nhận được
 * Return: không có
 */
void alert_handle(AlertCtx *ctx, const AlarmMsg *msg)
{
    char ts[32];
    const char *unit = (msg->type == ALARM_HUMID_HIGH) ? "%" : "C";

    if (msg->type < 0 || msg->type >= ALARM_TYPE_COUNT)
        return;
    ctx->counts[msg->type]++;
    ctx->total++;

    now_wall_str(ts, sizeof(ts));
    fprintf(stderr, "[ALERT][%s] TYPE: %s VALUE: %.1f%s THRESHOLD: %.1f%s (sample #%u)\n",
            ts, alarm_type_name(msg->type), msg->value, unit,
            msg->threshold, unit, msg->seq);
    log_post("[alert] TYPE: %s VALUE: %.1f%s THRESHOLD: %.1f%s sample=#%u ts=%lld",
             alarm_type_name(msg->type), msg->value, unit,
             msg->threshold, unit, msg->seq, msg->ts_ms);
}

/*
 * alert_thread_main - vòng lặp của Alert Thread
 * @arg: AlertCtx*
 * Return: NULL
 */
void *alert_thread_main(void *arg)
{
    AlertCtx *ctx = arg;
    AlarmMsg msg;

    pthread_setname_np(pthread_self(), "sms_alert");
    log_post("[alert] thread started, waiting on %s", SMS_ALARM_MQ_NAME);

    for (;;) {
        ssize_t n = mq_receive(ctx->alarm_q, (char *)&msg, sizeof(msg), NULL);

        if (n < 0) {
            if (errno == EINTR)
                continue;
            fprintf(stderr, "[alert] mq_receive: %s\n", strerror(errno));
            break;
        }
        if (msg.type == ALARM_SHUTDOWN)
            break;
        alert_handle(ctx, &msg);
    }

    log_post("[alert] thread stopped (TEMP_HIGH=%lu, HUMID_HIGH=%lu, TEMP_SPIKE=%lu)",
             ctx->counts[ALARM_TEMP_HIGH], ctx->counts[ALARM_HUMID_HIGH],
             ctx->counts[ALARM_TEMP_SPIKE]);
    return NULL;
}
