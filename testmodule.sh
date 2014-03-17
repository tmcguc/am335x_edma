sudo cp ./dma_drvr.ko /lib/modules/3.8.13-bone40/build/drivers/dma/
sudo depmod -a 
sudo insmod /lib/modules/3.8.13-bone40/build/drivers/dma/dma_drvr.ko
sleep 5
sudo rmmod /lib/modules/3.8.13-bone40/build/drivers/dma/dma_drvr.ko
dmesg | tail
