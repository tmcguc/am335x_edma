
/*
 * Mighty EBIC DMA Driver
 * 	version 0.1
 *   
 *  STABLE RELEASE: Registers as a working platform driver/device with the Linux Kernel
 * 		while providing functions for allocating kernel memory, DMA pools, and handling
 * 		the setup of EDMA3 Controller functions for the Beaglebone Black. Current
 * 		configuration is specific to the Mighty EBIC, review channels, slots and PaRAM
 * 		sets before using.
 *
 *  Andrew Righter <q@crypto.com>
 *  Ephemeron Labs, Inc.
 * 	February 5, 2014
*/

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

#include <linux/platform_device.h>			/* Register/Unregister Platform Drivers */
#include <linux/platform_data/edma.h>   	/* edmacc_param type */

#include "include/virt-dma.h"
#include "include/mighty_dma.h"
#include "include/dmaengine.h"

static struct dma_pool *pool_a;				/* Required for dma_pool_create */
static struct dma_pool *pool_b;

/*
static inline struct edma_chan *to_edma_chan(struct dma_chan *c) {
	return container_of(c, struct edma_chan, vchan.chan);
}
*/

static void edma_execute(struct edma_cc *ecc) {

	struct edma_desc *edesc;

	edesc = ecc->edesc;
	edesc->src_port = 0x480301a0;

	// change slot[0] to PINGSET and slot[1] to PONGSET
	edma_set_src(ecc->slot[0], edesc->src_port, FIFO, W32BIT);
	//edma_set_src(ecc->slot[1], edesc->src_port, FIFO, W32BIT);

	edma_set_dest(ecc->slot[0], ecc->kmem_addr_a->src_addr, FIFO, W32BIT);
	//edma_set_dest(ecc->slot[1], ecc->kmem_addr_b->src_addr, FIFO, W32BIT);

	edma_set_src_index(ecc->slot[0], 0, 0);
	edma_set_dest_index(ecc->slot[0], 4, 32);

	//edma_set_src_index(ecc->slot[1], 0, 0);
	//edma_set_dest_index(ecc->slot[1], 4, 32);

	//edma_link(ecc->slot[0], ecc->slot[1]);

	edma_read_slot(ecc->slot[0], &edesc->pingset);
	printk(KERN_INFO "%d:, opt=%x, src=%x, a_b_cnt=%x dst=%x\n", ecc->slot[0], pingset.opt, pingset.src, pingset.a_b_cnt, pingset.dst);
	
	edma_write_slot(ecc->slot[0], &edesc->pingset);
	printk(KERN_INFO "%d:, opt=%x, src=%x, a_b_cnt=%x dst=%x\n", ecc->slot[0], pingset.opt, pingset.src, pingset.a_b_cnt, pingset.dst);


	//edma_write_slot(ecc->slot[1], &edesc->pongset);
	//edma_write_slot(ecc->slot[0], &edesc->pset[17]);
}

static void dma_callback(unsigned link, u16 ch_status, void *data) {

	struct edma_cc *ecc = data;
	struct edma_desc *edesc;

	edesc = ecc->edesc;
	printk(KERN_INFO "dis shit running? bad!\n");
	if (edesc) {
		edma_execute(ecc);///TODO: edma execute isn't called before this src and dest not set untill called back transfer doesn't happen,  even triggered but goes nowhere
	}
	else {
		printk(KERN_INFO "Could not run edma_execute\n");
	}

#ifdef DEBUG
	printk(KERN_INFO "EDMA transfer test: link=%d, status=0x%x\n", link, ch_status);
#endif
	if (unlikely(ch_status != DMA_COMPLETE))
		return;
}

/*
	//struct edma_chan *echan = to_edma_chan(chan);
	//struct device *dev = chan->device->dev;

	... DIS SOME BULLSHIT

	//echan->alloced = true;
	//echan->slot[0] = a_ch_num;
	ecc->slot[0] = a_ch_num;
}
*/

static int edma_dma_init(struct edma_cc *ecc) {

	int dma_alloc_channel = 17;
	int ret;

	printk(KERN_INFO "INSIDE edma_dma_init\n");
	//edma_alloc_chan_resources(ecc);

	ret = edma_alloc_channel(dma_alloc_channel, dma_callback, ecc, EVENTQ_DEFAULT); // was EVENTQ_0
	if (ret < 0) {
		printk(KERN_INFO "edma_alloc_channel failed\n");
		return -EINVAL;
	}
	printk(KERN_INFO "edma channel allocated:%d\n", ret);

	ecc->slot[0] = ret;

	return 0;
}


static int edma_probe(struct platform_device *pdev) {

	struct edma_cc *ecc;

	struct ebic_dmac *dmac_a;
	struct ebic_dmac *dmac_b;

	struct mem_addr *kmem_addr_a;
	struct mem_addr *kmem_addr_b;

	int ret;

#ifdef DEBUG
	printk(KERN_INFO "Entering edma_probe function\n");
	printk(KERN_INFO "Allocating Memory for DMA\n");
#endif

	ecc = devm_kzalloc(&pdev->dev, sizeof(*ecc), GFP_KERNEL);
	if (!ecc) {
		printk(KERN_INFO "ecc memory can not be allocated\n");
		return -ENOMEM;
	}

	ecc->ctlr = pdev->id;

	/* 
	 * 		Enable this code only if you need a dummy_slot
	 * 		all PaRAM slots <64 use DCHMAP function
	ecc->slot_17 = edma_alloc_slot(ecc->ctlr, EDMA_SLOT_ANY);
	if (ecc->slot_17 < 0) {
		printk(KERN_INFO "Can't allocate PaRAM Slot 17\n");
		return -EIO;
	}
	* 
	*/

#ifdef DEBUG
	//printk(KERN_INFO "Allocated PaRAM Slot: %d\n", ecc->slot_17);
	printk(KERN_INFO "ecc->ctlr: %d\n", ecc->ctlr);
#endif
	
	dmac_a = kzalloc(sizeof(struct ebic_dmac), GFP_KERNEL);
	dmac_b = kzalloc(sizeof(struct ebic_dmac), GFP_KERNEL);

	if (!dmac_a || !dmac_b) {
		printk(KERN_ERR "%s: failed to alloc mem for dmacs\n", __func__);
		return -ENOMEM;
	}

	ecc->dmac_a = dmac_a;	
	ecc->dmac_b = dmac_b;

#ifdef DEBUG
	printk(KERN_INFO "dmac_a: %p\n", ecc->dmac_a);
	printk(KERN_INFO "dmac_b: %p\n", ecc->dmac_b);
#endif

	// DMA_POOL_SIZE will be dynamically sized based on how many channels of data we have (multiple of 4)
	pool_a = dma_pool_create("pool_a memtest", NULL, DMA_POOL_SIZE, 16, 0);
	pool_b = dma_pool_create("pool_b memtest", NULL, DMA_POOL_SIZE, 16, 0);

	if (!pool_a || !pool_b) {
		printk(KERN_ERR "%s: failed to create pools\n", __func__);
		return -ENOMEM;
	}

	printk(KERN_INFO "pool_a: %p\n", pool_a);
	printk(KERN_INFO "pool_b: %p\n", pool_b);

	kmem_addr_a = dma_pool_alloc(pool_a, GFP_ATOMIC, &dmac_a->dma_handle);
	kmem_addr_b = dma_pool_alloc(pool_b, GFP_ATOMIC, &dmac_b->dma_handle);

	if (!kmem_addr_a || !kmem_addr_b) {
		printk(KERN_ERR "%s: failed to allocate memory for kmem_addrs\n", __func__);
		return -ENOMEM;
	}

	ecc->kmem_addr_a = kmem_addr_a;
	ecc->kmem_addr_b = kmem_addr_b;

#ifdef DEBUG
	printk(KERN_INFO "ecc->kmem_addr_a: %p\n", ecc->kmem_addr_a);
	printk(KERN_INFO "ecc->kmem_addr_a->src_addr: %08x\n", ecc->kmem_addr_a->src_addr);
	printk(KERN_INFO "ecc->kmem_addr_a->dst_addr: %08x\n", ecc->kmem_addr_a->dst_addr);
	printk(KERN_INFO "ecc->kmem_addr_b: %p\n", ecc->kmem_addr_b);
#endif

	edma_dma_init(ecc);

	platform_set_drvdata(pdev, ecc);

	printk(KERN_INFO "before edma_execute\n");
	edma_execute(ecc);
	printk(KERN_INFO "after edma_execute\n");

	ret = edma_start(ecc->slot[0]);
	printk(KERN_INFO "edma started on channel:%d\n", ecc->slot[0]);
	if (ret != 0) {
		printk(KERN_INFO "edma_start failed\n");
		return ret;
	}

	return 0;
}

static int edma_remove(struct platform_device *pdev) {

	struct device *dev = &pdev->dev;
	struct edma_cc *ecc = dev_get_drvdata(dev);						/* !!! from drivers/base/dd.c L578, if (dev && dev->p) return dev->p->driver_data; !!! */

	printk(KERN_INFO "Freeing all dma_pool memory\n");

	edma_stop(ecc->slot[0]);
	edma_free_slot(ecc->slot[0]);
	
	dma_pool_free(pool_a, &ecc->kmem_addr_a, ecc->dmac_a->dma_handle);
	dma_pool_free(pool_b, &ecc->kmem_addr_b, ecc->dmac_b->dma_handle);
	kfree(ecc->dmac_a);
	kfree(ecc->dmac_b);
	dma_pool_destroy(pool_a);
	dma_pool_destroy(pool_b);

	return 0;
}

static struct platform_driver edma_driver = {
	.probe = edma_probe,
	.remove	= edma_remove,
	.driver = {
		.name = "mighty-ebic-dma",
		.owner = THIS_MODULE,
	},

};

static struct platform_device *pdev0;

static const struct platform_device_info edma_dev_info0 = {
	.name = "mighty-ebic-dma",
	.id = 0,
	.dma_mask = DMA_BIT_MASK(32),
};

static int __init dma_init(void) {

	int ret = platform_driver_register(&edma_driver);

	if (ret == 0) {
		pdev0 = platform_device_register_full(&edma_dev_info0);
		if (IS_ERR(pdev0)) {
			platform_driver_unregister(&edma_driver);
			ret = PTR_ERR(pdev0);
			goto out;
		}
	}

out:
	return ret;
}
subsys_initcall(dma_init);

static void __exit dma_exit(void) {

	printk(KERN_INFO "Unregistering Device and Driver\n");

	platform_device_unregister(pdev0);
	platform_driver_unregister(&edma_driver);

}
module_exit(dma_exit);
 
MODULE_AUTHOR("Ephemeron Labs Inc.");
MODULE_DESCRIPTION("Mighty EBIC DMA Driver");
MODULE_LICENSE("GPL");
