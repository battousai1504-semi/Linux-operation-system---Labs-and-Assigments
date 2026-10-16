KDIR := $(HOME)/embedded_lab1_clean/build/kernel
ARCH := arm
CROSS_COMPILE := arm-linux-gnueabihf-

obj-m += lab2_driver.o

.PHONY: all clean

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) \
		ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) \
		CC=arm-linux-gnueabihf-gcc-11 HOSTCC=gcc-11 \
		KCFLAGS= KAFLAGS= modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) \
		ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean
