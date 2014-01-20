#ifndef _DAVINCI_PCM_H
#define _DAVINCI_PCM_H

#include <linux/genalloc.h>
/* #include <linux/platform_data/davinci_asp.h> */
#include <linux/platform_data/edma.h>

/* struct davinci_pcm_dma_params { */
struct dma_params {
	int channel;						/* sync dma channel ID */
	unsigned short acnt;
	dma_addr_t dma_addr;					/* device physical address for DMA */
	unsigned sram_size;
	struct gen_pool *sram_pool;				/* SRAM gen_pool for ping pong */
	enum dma_event_q asp_chan_q;				/* event queue number for ASP channel */
	enum dma_event_q ram_chan_q;				/* event queue number for RAM channel */
	unsigned char data_type;				/* xfer data type */
	//unsigned char convert_mono_stereo;
	unsigned int fifo_level;
};

/*
int davinci_soc_platform_register(struct device *dev);
void davinci_soc_platform_unregister(struct device *dev);
*/

int transfer_platform_register(struct device *dev);
void transfer_platform_unregister(struct device *dev);
#endif

