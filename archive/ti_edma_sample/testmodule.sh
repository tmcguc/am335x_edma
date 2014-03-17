sudo cp ./ti_edma_sample.ko /lib/modules/3.8.13-bone40/build/drivers/dma/
sudo depmod -a 
sudo insmod /lib/modules/3.8.13-bone40/build/drivers/dma/ti_edma_sample.ko
sleep 5
sudo rmmod /lib/modules/3.8.13-bone40/build/drivers/dma/ti_edma_sample.ko
dmesg | tail
