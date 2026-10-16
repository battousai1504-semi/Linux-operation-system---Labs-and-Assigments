/*
 * sms_common.h - Kiểu dữ liệu, hằng số và hàm tiện ích dùng chung của sms_app
 *
 * Author : Nguyen Tran Quang - SE200988
 * Date   : 2026-10-16
 */
#ifndef SMS_COMMON_H
#define SMS_COMMON_H

#include <stdatomic.h>
#include <mqueue.h>
#include <time.h>
#include "sms_sensor.h"          /* SMS_DEVICE_PATH, ioctl (thư mục driver/) */

/* ------------------------- Cấu hình mặc định ------------------------- */
#define SMS_LOG_PATH          "/tmp/sms.log"
#ifndef SMS_APP_DEFAULT_DEV                    /* bản x86: -DSMS_APP_DEFAULT_DEV='"sim"' */
#define SMS_APP_DEFAULT_DEV   SMS_DEVICE_PATH
#endif
#define SMS_ALARM_MQ_NAME     "/sms_alarm"     /* POSIX mq: Processor -> Alert  */
#define SMS_LOG_MQ_NAME       "/sms_log"       /* POSIX mq: mọi thread -> Logger */
#define SMS_MQ_MAXMSG         10               /* giới hạn mặc định cho user thường */

#define CB_CAPACITY           64               /* số phần tử circular buffer */
#define ROLL_WINDOW           10               /* cửa sổ rolling average     */
#define TEMP_SPIKE_DELTA      5.0              /* °C so với mẫu trước        */
#define LOG_MSG_MAX           200              /* độ dài 1 dòng log          */
#define LOG_ROTATE_BYTES      (1024L * 1024L)  /* 1 MB                       */
#define LOG_FLUSH_SEC         5
#define JITTER_MAX_SAMPLES    100000
#define SENSOR_RT_PRIO        50
#define THREAD_STACK_SIZE     (256 * 1024)
#define READ_RETRY_MAX        5                /* số lần thử lại khi EAGAIN/EINTR */

/* ------------------------- Kiểu dữ liệu ------------------------- */
typedef struct {
    unsigned int seq;          /* số thứ tự mẫu trong app        */
    long long ts_ms;           /* timestamp từ driver (ms)       */
    double temperature;        /* °C                             */
    double humidity;           /* %                              */
} SensorData;

typedef enum {
    ALARM_TEMP_HIGH = 0,
    ALARM_HUMID_HIGH,
    ALARM_TEMP_SPIKE,
    ALARM_TYPE_COUNT,
    ALARM_SHUTDOWN = 99        /* message đặc biệt: báo Alert Thread dừng */
} AlarmType;

typedef struct {
    int type;                  /* AlarmType */
    unsigned int seq;
    long long ts_ms;
    double value;
    double threshold;
} AlarmMsg;

typedef struct {
    int shutdown;              /* 1 = báo Logger Thread dừng */
    char text[LOG_MSG_MAX];
} LogMsg;

typedef struct {
    int interval_ms;           /* -i */
    double temp_th;            /* -t */
    double hum_th;             /* -h */
    int duration_s;            /* -d, 0 = chạy tới khi Ctrl+C */
    int verbose;               /* -v */
    int use_fifo;              /* -s fifo|other */
    const char *dev_path;      /* -D */
    const char *log_path;      /* -L */
} AppConfig;

/* Cờ chạy toàn cục: 1 = đang chạy, 0 = đang shutdown (atomic, không cần mutex) */
extern atomic_int g_running;

/* Tên loại alarm */
const char *alarm_type_name(int type);

/* ------------------------- Tiện ích thời gian ------------------------- */

/* ts_add_ms - cộng ms vào timespec (chuẩn hóa tv_nsec) */
static inline void ts_add_ms(struct timespec *t, long ms)
{
    t->tv_sec += ms / 1000;
    t->tv_nsec += (ms % 1000) * 1000000L;
    if (t->tv_nsec >= 1000000000L) {
        t->tv_sec++;
        t->tv_nsec -= 1000000000L;
    }
}

/* timespec_diff_us - a - b tính bằng micro giây */
static inline long timespec_diff_us(const struct timespec *a, const struct timespec *b)
{
    return (long)((a->tv_sec - b->tv_sec) * 1000000L + (a->tv_nsec - b->tv_nsec) / 1000L);
}

/* now_wall_str - chuỗi thời gian thực "YYYY-mm-dd HH:MM:SS.mmm" */
void now_wall_str(char *buf, int len);

#endif /* SMS_COMMON_H */
