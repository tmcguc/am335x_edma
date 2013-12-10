EDMA Driver
===========

This chapter provides details on EDMA driver along with throughput and CPU load numbers.

Introduction
------------

The EDMA controller handles all data transfers between the level-two (L2) cache/memory controller and the 
device peripherals. On AM335x EDMA has has one instance of Channel controller. Each EDMA instance supports 
up to 32-dma channels and 8 QDMA channels. The EDMA consists of a scalable Parameter RAM (PaRAM) that supports 
flexible ping-pong, circular buffering, channel-chaining, auto-reloading, and memory protection. The EDMA 
allows movement of data to/from any addressable memory spaces, including internal memory (L2 SRAM), peripherals, 
and external memory.

The EDMA driver exposes only the kernel level API's. This driver is used as a utility by other drivers for data transfer.

Driver Features
---------------

The driver supports the following features:

*    Request and Free DMA channel
*    Programs DMA channel
*    Start and Synchronize with DMA transfers
*    Provides DMA transaction completion callback to applications
*    Multiple instances of EDMA driver on a single processor 

### Features Not Supported

*	QDMA is not supported.
*	Reservation of resources (channels and PaRAMs) for usage from other masters is not supported. 

### References

*	[AM335x-PSP Features and Performance Guide](http://processors.wiki.ti.com/index.php/AM335x-PSP_04.06.00.08_Features_and_Performance_Guide#EDMA_Driver)

