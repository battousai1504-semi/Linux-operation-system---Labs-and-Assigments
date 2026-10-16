# Makefile - build sms_app
# Author : Nguyen Tran Quang - SE200988
#   make arm   : sms_app      (ARMv7 static, chạy trên QEMU)
#   make host  : sms_app_x86  (x86_64, -g -O0, mặc định -D sim; dùng cho GDB/Valgrind/strace/perf)
#   make       : build cả hai
#   make clean : xóa binary

CC_ARM   ?= arm-linux-gnueabihf-gcc
CC_HOST  ?= gcc
DRV_DIR  ?= ../driver
CFLAGS   := -Wall -Wextra -std=gnu11 -D_GNU_SOURCE -I$(DRV_DIR)
LDLIBS   := -pthread -lrt -lm
SRCS     := sms_app.c sensor_thread.c processor_thread.c alert_thread.c logger_thread.c circular_buffer.c
HDRS     := sms_common.h circular_buffer.h sensor_thread.h processor_thread.h alert_thread.h logger_thread.h $(DRV_DIR)/sms_sensor.h

all: arm host

arm: sms_app

host: sms_app_x86

sms_app: $(SRCS) $(HDRS)
	$(CC_ARM) $(CFLAGS) -O2 -static -o $@ $(SRCS) $(LDLIBS)

sms_app_x86: $(SRCS) $(HDRS)
	$(CC_HOST) $(CFLAGS) -O0 -g -fno-omit-frame-pointer -DSMS_APP_DEFAULT_DEV='"sim"' -o $@ $(SRCS) $(LDLIBS)

clean:
	rm -f sms_app sms_app_x86

.PHONY: all arm host clean
