sudo cp ./memtest.ko /lib/modules/3.8.13-bone28/kernel/drivers/dma/
sudo depmod -a 
sudo modprobe memtest
sleep 5
sudo modprobe -r memtest
dmesg | tail
