/*
 * alert_thread.h - Alert Thread: nhận alarm từ alarm_queue (POSIX mq)
 *
 * Author : Nguyen Tran Quang - SE200988
 * Date   : 2026-10-16
 */
#ifndef ALERT_THREAD_H
#define ALERT_THREAD_H

#include "sms_common.h"

typedef struct {
    mqd_t alarm_q;                          /* mq "/sms_alarm" (blocking) */
    unsigned long counts[ALARM_TYPE_COUNT]; /* số alert theo từng loại     */
    unsigned long total;
} AlertCtx;

void alert_handle(AlertCtx *ctx, const AlarmMsg *msg);
void *alert_thread_main(void *arg);

#endif /* ALERT_THREAD_H */
