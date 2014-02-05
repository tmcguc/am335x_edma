#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/dmapool.h>
#include <linux/dma-mapping.h>	
#include <linux/dmaengine.h>
#include <linux/genalloc.h>	
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/list.h>

#include <linux/platform_device.h>		/* Register/Unregister Platform Drivers */
#include <linux/platform_data/edma.h>   /* edmacc_param type */

#include "virt-dma.h"

#define EDMA_CTLRS	2
#define EDMA_CHANS	32

/* Max of 16 segments per channel to conserve PaRAM slots */
#define MAX_NR_SG			16
#define EDMA_MAX_SLOTS		MAX_NR_SG
#define EDMA_DESCRIPTORS	16

#define DEBUG			1	
#define PINGCHAN		20
#define PONGCHAN		64
#define DMA_POOL_SIZE	4096

/* DMA memory addresses */
struct mem_addr {
	u32	src_addr;
	u32	dst_addr;
};

/* from d/d/edma.c */
struct edma_desc {
	//struct virt_dma_desc	vdesc;
	//struct list_head		node;
	int						absync;
	int						pset_nr;
	struct edmacc_param		pset[0];
};

struct edma_cc;

struct edma_chan {
	struct virt_dma_chan	vchan;
	//struct list_head		node;
	struct edma_desc		*edesc;
	struct edma_cc			*ecc;
	int						ch_num;
	bool					alloced;
	int						slot[EDMA_MAX_SLOTS];
	//struct dma_slave_config		cfg;
	//struct dmaengine_chan_caps	caps;
};

struct edma_cc {
	int						ctlr;
	//struct dma_device		dma_slave;
	struct edma_chan		slave_chans[EDMA_CHANS];
	int						num_slave_chans;
	int						slot_20;			/* TESTING */
	int 					slot_17;			/* FOR SPI TRIGGER */
	
	struct ebic_dmac		*dmac_a;
	struct ebic_dmac		*dmac_b;

	struct mem_addr			*kmem_addr_a;		/* Memory Address returned from dma_pool_alloc */
	struct mem_addr			*kmem_addr_b;

};

/* EBIC DMA Channel Structure */
struct ebic_dmac {
	dma_addr_t		dma_handle;		/* DMA Handle set by dma_pool_alloc */

	struct edmacc_param	*dma_test_params;
	struct edmacc_param	*pingset;
	struct edmacc_param	*pongset;

	spinlock_t		lock;
	int				dma_ch;
	int				dma_link[2];
	int				slot;
};

/*
static struct edmacc_param pingset = {
	.opt = 0,
	.src = 0,
	.a_b_cnt = 0,
	.dst = 0,
	.src_dst_bidx = 0,
	.link_bcntrld = 0,
	.src_dst_cidx = 0,
	.ccnt = 0,
};


static struct edmacc_param pongset = {
	.opt = 0,
	.src = 0,
	.a_b_cnt = 0,
	.dst = 0,
	.src_dst_bidx = 0,
	.link_bcntrld = 0,
	.src_dst_cidx = 0,
	.ccnt = 0,
};
*/




