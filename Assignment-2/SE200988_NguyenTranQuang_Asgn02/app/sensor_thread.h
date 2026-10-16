/*
 * sensor_thread.h - Sensor Thread: đọc /dev/sms_sensor theo chu kỳ, SCHED_FIFO
 *
 * Author : Nguyen Tran Quang - SE200988
 * Date   : 2026-10-16
 */
#ifndef SENSOR_THREAD_H
#define SENSOR_THREAD_H

#include <stdio.h>
#include "sms_common.h"
#include "circular_buffer.h"

typedef struct {
    int fd;                          /* fd của /dev/sms_sensor (main mở/đóng) */
    int sim;                         /* 1 = -D sim: bộ mô phỏng trong app (x86 host) */
    unsigned int sim_seed;
    int sim_t_x10, sim_h_x10;        /* trạng thái random walk của bộ mô phỏng */
    const AppConfig *cfg;
    CircularBuffer *cb;              /* producer                         */
    int *jitter_us;                  /* mảng jitter cấp phát sẵn bởi main */
    int jitter_cap;
    int jitter_count;
    int policy;                      /* policy thực tế sau khi set       */
    int prio;
    unsigned int seq;
    unsigned long reads_ok, eagain, eintr, eof, read_errors, parse_errors;
    unsigned long dropped;           /* buffer đầy -> bỏ mẫu             */
    unsigned long overruns;          /* trễ quá 1 chu kỳ                  */
} SensorCtx;

void *sensor_thread_main(void *arg);
void sensor_print_jitter_report(SensorCtx *s, FILE *out);

#endif /* SENSOR_THREAD_H */
