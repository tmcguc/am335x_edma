sudo cp ./dma_drvr.ko /lib/modules/3.8.13-bone40/build/drivers/dma/
sudo depmod -a 
sudo modprobe dma_drvr
sleep 5
sudo modprobe -r dma_drvr
dmesg | tail
