obj-m += edma-ebic.o

all:
	make -I /home/ubuntu/am335x_edma/include -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
