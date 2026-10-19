/*
 * sms_sensor.h - Header dung chung cho driver va ung dung (ASSIGNMENT-02)
 * Author : Nguyen Tran Quang - SE200988
 * Date   : 2026-10-16
 *
 * Chi chua dinh nghia ioctl, giong cach lam asgn1_ioctl.h o ASSIGNMENT-01.
 */
#ifndef SMS_SENSOR_H
#define SMS_SENSOR_H

#include <linux/ioctl.h>

#define SMS_MAGIC 'S'

/* Dat khoang thoi gian toi thieu giua 2 lan doc (don vi ms, mac dinh 100) */
#define SMS_SET_INTERVAL _IOW(SMS_MAGIC, 1, int)

#endif
