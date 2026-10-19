/* sms_sensor.h - Header dung chung cho driver va ung dung (ASSIGNMENT-02)
 * Author : Nguyen Tran Quang - SE200988
 * Date   : 2026-10-16 */
#ifndef SMS_SENSOR_H
#define SMS_SENSOR_H

#include <linux/ioctl.h>

#define SMS_MAGIC 'S'

#define SMS_SET_INTERVAL _IOW(SMS_MAGIC, 1, int)

#endif
