obj-m += asgn1_driver.o

KDIR := $(HOME)/embedded_lab1_clean/build/kernel
ARCH := arm
CROSS_COMPILE := arm-linux-gnueabihf-

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) \
		ARCH=$(ARCH) \
		CROSS_COMPILE=$(CROSS_COMPILE) \
		CC=arm-linux-gnueabihf-gcc-11 \
		HOSTCC=gcc-11 \
		KCFLAGS= KAFLAGS= modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
