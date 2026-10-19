# Makefile - build sms_sensor_driver.ko cho ARMv7 (QEMU vexpress-a9)

KDIR ?= $(HOME)/embedded_lab1/kernel/linux-5.15
ARCH := arm
CROSS_COMPILE ?= arm-linux-gnueabihf-

obj-m += sms_sensor_driver.o
ccflags-y += -march=armv7-a

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean
