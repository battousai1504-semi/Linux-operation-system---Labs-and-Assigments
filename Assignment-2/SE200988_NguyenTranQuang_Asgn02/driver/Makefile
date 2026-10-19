# Makefile - build sms_sensor_driver.ko cho ARMv7 (QEMU vexpress-a9)
# Author : Nguyen Tran Quang - SE200988
# KDIR = kernel source da build o LAB-01 (co the doi: make KDIR=/duong/dan)

KDIR ?= $(HOME)/embedded_lab1/kernel/linux-5.15
ARCH := arm
CROSS_COMPILE ?= arm-linux-gnueabihf-

obj-m += sms_sensor_driver.o
ccflags-y += -march=armv7-a

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean
