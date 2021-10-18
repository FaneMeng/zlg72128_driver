LINUX_KERNEL_PATH=/home/mfy/Linux-drive/Linux-core-source-code/linux-imx-rel_imx_4.1.15_2.1.0_ga/
CROSS_COMPILE=/usr/local/arm/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-
ARCH=arm

obj-m += zlg72128_device.o
all:
	make -C $(LINUX_KERNEL_PATH) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) M=$(CURDIR) modules
clean:
	make -C $(LINUX_KERNEL_PATH) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) M=$(CURDIR) modules clean
	rm -rf *.o *.ko *.mod.c *.symvers *.order
