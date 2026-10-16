/* SPDX-License-Identifier: GPL-2.0 */
/*
 * sms_sensor.h - Header dùng chung cho sms_sensor_driver.c (kernel)
 *                và sms_app (userspace): tên thiết bị, ioctl, data struct.
 *
 * Author: Nguyen Tran Quang - SE200988
 * Date  : 2026-10-16
 */
#ifndef SMS_SENSOR_H
#define SMS_SENSOR_H

#include <linux/types.h>        /* __u64, __s32 (dùng được cả 2 phía) */
#ifdef __KERNEL__
#include <linux/ioctl.h>
#else
#include <sys/ioctl.h>
#endif

#define SMS_DEVICE_NAME          "sms_sensor"        /* -> /dev/sms_sensor */
#define SMS_DEVICE_PATH          "/dev/sms_sensor"
#define SMS_PROC_NAME            "sms_stats"         /* -> /proc/sms_stats */

/* Một lần read() trả đúng 1 dòng CSV: "<timestamp_ms>,<temp>,<humid>\n" */
#define SMS_LINE_MAX             64

#define SMS_DEFAULT_INTERVAL_MS  100   /* khoảng tối thiểu giữa 2 lần đọc */
#define SMS_MIN_INTERVAL_MS      1
#define SMS_MAX_INTERVAL_MS      60000

/* Dải giá trị mô phỏng, đơn vị 0.1 (kernel không dùng số thực) */
#define SMS_TEMP_MIN_X10         150   /* 15.0 °C */
#define SMS_TEMP_MAX_X10         450   /* 45.0 °C */
#define SMS_HUMID_MIN_X10        300   /* 30.0 %  */
#define SMS_HUMID_MAX_X10        900   /* 90.0 %  */

/* Một mẫu cảm biến như driver lưu bên trong (fixed-point x10) */
struct sms_raw_sample {
    __u64 timestamp_ms;   /* thời điểm đọc, ms tính từ lúc boot   */
    __s32 temp_x10;       /* nhiệt độ x10, vd 284 = 28.4 °C      */
    __s32 humid_x10;      /* độ ẩm x10,    vd 615 = 61.5 %       */
};

/* ioctl - magic 'Y' (chưa dùng trong ioctl-number.rst của kernel 5.15) */
#define SMS_IOC_MAGIC            'Y'
#define SMS_SET_INTERVAL         _IOW(SMS_IOC_MAGIC, 1, int)   /* ms */
#define SMS_GET_INTERVAL         _IOR(SMS_IOC_MAGIC, 2, int)   /* ms */

#endif /* SMS_SENSOR_H */
