/*
 * circular_buffer.h - Circular buffer an toàn đa luồng (mutex + condition variable)
 *
 * Author : Nguyen Tran Quang - SE200988
 * Date   : 2026-10-16
 */
#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include <pthread.h>
#include "sms_common.h"

typedef struct {
    SensorData *items;               /* mảng cấp phát 1 lần trên heap   */
    int capacity;
    int head;                        /* vị trí lấy ra tiếp theo         */
    int tail;                        /* vị trí ghi vào tiếp theo        */
    int size;                        /* số phần tử hiện có              */
    unsigned long total_produced;
    unsigned long total_consumed;
    unsigned long overflow_count;    /* số mẫu bị bỏ vì buffer đầy      */
    int shutdown;                    /* 1 = không nhận thêm, đánh thức consumer */
    pthread_mutex_t lock;            /* bảo vệ TẤT CẢ trường ở trên     */
    pthread_cond_t not_empty;        /* consumer chờ khi buffer rỗng    */
} CircularBuffer;

typedef struct {
    int current_size;
    int capacity;
    unsigned long total_produced;
    unsigned long total_consumed;
    unsigned long overflow_count;
} CbStats;

int  cb_init(CircularBuffer *cb, int capacity);
void cb_destroy(CircularBuffer *cb);
int  cb_push(CircularBuffer *cb, const SensorData *d);
int  cb_pop(CircularBuffer *cb, SensorData *out);
void cb_shutdown(CircularBuffer *cb);
void cb_get_stats(CircularBuffer *cb, CbStats *st);

#endif /* CIRCULAR_BUFFER_H */
