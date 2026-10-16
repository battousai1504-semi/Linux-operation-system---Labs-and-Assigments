/*
 * logger_thread.h - Logger Thread: nhận log qua POSIX mq, ghi ra file có rotate
 *
 * Author : Nguyen Tran Quang - SE200988
 * Date   : 2026-10-16
 */
#ifndef LOGGER_THREAD_H
#define LOGGER_THREAD_H

#include "sms_common.h"

typedef struct {
    const char *path;            /* đường dẫn log (mặc định SMS_LOG_PATH) */
    unsigned long lines;         /* số dòng đã ghi            */
    unsigned long flushes;       /* số lần flush định kỳ      */
    unsigned long rotations;     /* số lần rotate file        */
} LoggerCtx;

int  log_queue_open(void);
void log_queue_close(void);
void log_post(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
int  logger_send_shutdown(void);
unsigned long log_posted_count(void);
unsigned long log_dropped_count(void);
void *logger_thread_main(void *arg);

#endif /* LOGGER_THREAD_H */
