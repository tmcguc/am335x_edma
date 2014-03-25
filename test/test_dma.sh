sudo cp ./mighty_dma.ko /lib/modules/3.8.13-bone40/build/drivers/dma/
sudo depmod -a 
sudo insmod /lib/modules/3.8.13-bone40/build/drivers/dma/mighty_dma.ko
sleep 5
sudo rmmod /lib/modules/3.8.13-bone40/build/drivers/dma/mighty_dma.ko
dmesg | tail
