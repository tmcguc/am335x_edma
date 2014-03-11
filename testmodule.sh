sudo cp ./mighty_dma.ko /lib/modules/3.8.13-bone40/build/drivers/dma/
sudo depmod -a 
sudo modprobe mighty_dma
sleep 5
sudo modprobe -r mighty_dma
dmesg | tail
