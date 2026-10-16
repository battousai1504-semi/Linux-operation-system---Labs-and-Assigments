/*
 * circular_buffer.c - Circular buffer an toàn đa luồng cho SensorData
 *
 * Author : Nguyen Tran Quang - SE200988
 * Date   : 2026-10-16
 *
 * Producer (Sensor Thread) không bao giờ chờ: buffer đầy -> bỏ mẫu, tăng
 * overflow_count. Consumer (Processor Thread) chờ trên condition variable
 * khi buffer rỗng. Chỉ có 1 mutex -> không thể deadlock trong module này.
 */
#include <stdlib.h>
#include <string.h>
#include "circular_buffer.h"

/*
 * cb_init - cấp phát mảng (1 lần duy nhất) và khởi tạo mutex/cond
 * @cb:       buffer cần khởi tạo
 * @capacity: số phần tử tối đa
 * Return: 0 nếu thành công, -1 nếu hết bộ nhớ
 */
int cb_init(CircularBuffer *cb, int capacity)
{
    memset(cb, 0, sizeof(*cb));
    cb->items = calloc((size_t)capacity, sizeof(SensorData));
    if (!cb->items)
        return -1;
    cb->capacity = capacity;
    pthread_mutex_init(&cb->lock, NULL);
    pthread_cond_init(&cb->not_empty, NULL);
    return 0;
}

/*
 * cb_destroy - giải phóng mảng và hủy mutex/cond (gọi sau khi mọi thread đã join)
 * @cb: buffer
 * Return: không có
 */
void cb_destroy(CircularBuffer *cb)
{
    pthread_cond_destroy(&cb->not_empty);
    pthread_mutex_destroy(&cb->lock);
    free(cb->items);
    cb->items = NULL;
}

/*
 * cb_push - thêm 1 mẫu (không chờ)
 * @cb: buffer, @d: mẫu cần thêm
 * Return: 0 nếu thêm được, -1 nếu buffer đầy hoặc đang shutdown (mẫu bị bỏ)
 */
int cb_push(CircularBuffer *cb, const SensorData *d)
{
    int ret = 0;

    pthread_mutex_lock(&cb->lock);
    if (cb->shutdown || cb->size == cb->capacity) {
        cb->overflow_count++;
        ret = -1;
    } else {
        cb->items[cb->tail] = *d;
        cb->tail = (cb->tail + 1) % cb->capacity;
        cb->size++;
        cb->total_produced++;
        pthread_cond_signal(&cb->not_empty);
    }
    pthread_mutex_unlock(&cb->lock);
    return ret;
}

/*
 * cb_pop - lấy 1 mẫu, chờ (blocking) nếu buffer rỗng
 * @cb: buffer, @out: nơi nhận mẫu
 * Return: 0 nếu lấy được; -1 nếu đã shutdown và buffer rỗng (consumer nên thoát)
 */
int cb_pop(CircularBuffer *cb, SensorData *out)
{
    int ret = 0;

    pthread_mutex_lock(&cb->lock);
    while (cb->size == 0 && !cb->shutdown)
        pthread_cond_wait(&cb->not_empty, &cb->lock);   /* nhả lock khi chờ */

    if (cb->size == 0) {               /* shutdown và đã lấy hết */
        ret = -1;
    } else {
        *out = cb->items[cb->head];
        cb->head = (cb->head + 1) % cb->capacity;
        cb->size--;
        cb->total_consumed++;
    }
    pthread_mutex_unlock(&cb->lock);
    return ret;
}

/*
 * cb_shutdown - đánh dấu shutdown và đánh thức mọi consumer đang chờ
 * @cb: buffer
 * Return: không có
 */
void cb_shutdown(CircularBuffer *cb)
{
    pthread_mutex_lock(&cb->lock);
    cb->shutdown = 1;
    pthread_cond_broadcast(&cb->not_empty);
    pthread_mutex_unlock(&cb->lock);
}

/*
 * cb_get_stats - getter: chụp các bộ đếm trong cùng 1 lần giữ khóa
 * @cb: buffer, @st: nơi nhận thống kê
 * Return: không có
 */
void cb_get_stats(CircularBuffer *cb, CbStats *st)
{
    pthread_mutex_lock(&cb->lock);
    st->current_size = cb->size;
    st->capacity = cb->capacity;
    st->total_produced = cb->total_produced;
    st->total_consumed = cb->total_consumed;
    st->overflow_count = cb->overflow_count;
    pthread_mutex_unlock(&cb->lock);
}
