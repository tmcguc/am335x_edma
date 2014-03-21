obj-m += dma_drvr.o

all:
	make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- -C /home/mightydev/BBB/rootfs/lib/modules/3.8.13-bone41/build M=$(PWD) modules

clean:
	make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- -C /home/mightydev/BBB/rootfs/lib/modules/3.8.13-bone41/build M=$(PWD) clean
