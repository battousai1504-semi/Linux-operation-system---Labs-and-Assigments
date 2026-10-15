obj-m += asgn1_driver.o

KDIR := $(HOME)/embedded_lab1/kernel/linux-5.15

all:
	$(MAKE) -C $(KDIR) ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- M=$(CURDIR) modules

clean:
	$(MAKE) -C $(KDIR) ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- M=$(CURDIR) clean
