/*
 * processor_thread.h - Processor Thread: rolling average + phát hiện bất thường
 *
 * Author : Nguyen Tran Quang - SE200988
 * Date   : 2026-10-16
 */
#ifndef PROCESSOR_THREAD_H
#define PROCESSOR_THREAD_H

#include "sms_common.h"
#include "circular_buffer.h"

typedef struct {
    CircularBuffer *cb;              /* nguồn dữ liệu (consumer)        */
    mqd_t alarm_q;                   /* gửi alarm cho Alert Thread       */
    const AppConfig *cfg;
    double win_t[ROLL_WINDOW];       /* cửa sổ 10 mẫu gần nhất           */
    double win_h[ROLL_WINDOW];
    int win_count;
    int win_pos;
    double avg_t, avg_h;             /* rolling average hiện tại          */
    double prev_temp;
    int has_prev;
    unsigned long processed;
    unsigned long alarms_sent;
    unsigned long alarms_dropped;
} ProcessorCtx;

int processor_check_alarms(ProcessorCtx *p, unsigned int seq, long long ts_ms,
                           double temperature, double humidity);
void *processor_thread_main(void *arg);

#endif /* PROCESSOR_THREAD_H */
