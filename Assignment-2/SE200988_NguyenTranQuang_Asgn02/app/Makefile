# Makefile - build sms_app

CC_ARM  ?= arm-linux-gnueabihf-gcc
CC_HOST ?= gcc
CFLAGS  := -Wall -Wextra -D_GNU_SOURCE -I../driver
LDLIBS  := -pthread -lrt -lm
SRCS    := sms_app.c sensor_thread.c processor_thread.c alert_thread.c logger_thread.c circular_buffer.c
HDRS    := sms.h ../driver/sms_sensor.h

all: arm host

arm: sms_app

host: sms_app_x86

sms_app: $(SRCS) $(HDRS)
	$(CC_ARM) $(CFLAGS) -O2 -static -o $@ $(SRCS) $(LDLIBS)

sms_app_x86: $(SRCS) $(HDRS)
	$(CC_HOST) $(CFLAGS) -O0 -g -fno-omit-frame-pointer -o $@ $(SRCS) $(LDLIBS)

clean:
	rm -f sms_app sms_app_x86

.PHONY: all arm host clean
