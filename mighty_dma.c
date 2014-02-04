
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

#define DEBUG			1	
#define PINGCHAN		20
#define PONGCHAN		64
#define DMA_POOL_SIZE	4096

static struct dma_pool *pool_a;				/* Required for dma_pool_create */
static struct dma_pool *pool_b;

static inline struct edma_chan *to_edma_chan(struct dma_chan *c) {
	return container_of(c, struct edma_chan, vchan.chan);
}

// DMA CALLBACK

static int edma_probe(struct platform_device *pdev) {

	struct edma_cc *ecc;

	struct ebic_dmac *dmac_a;
	struct ebic_dmac *dmac_b;

	struct mem_addr *kmem_addr_a;
	struct mem_addr *kmem_addr_b;

	// how do i link dev->p to my ecc struct? 

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

	printk(KERN_INFO "ecc->ctlr: %d\n", ecc->ctlr);

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
	printk(KERN_INFO "ecc->kmem_addr_b: %p\n", ecc->kmem_addr_b);
#endif

	// Run the EDMA Allocate Channel function
	//edma_alloc_chan_resources;

	platform_set_drvdata(pdev, ecc);

	return 0;
}

static int edma_remove(struct platform_device *pdev) {

	struct device *dev = &pdev->dev;
	struct edma_cc *ecc = dev_get_drvdata(dev);						/* !!! from drivers/base/dd.c L578, if (dev && dev->p) return dev->p->driver_data; !!! */

	printk(KERN_INFO "Freeing all dma_pool memory\n");

	dma_pool_free(pool_a, &ecc->kmem_addr_a, ecc->dmac_a->dma_handle);
	dma_pool_free(pool_b, &ecc->kmem_addr_b, ecc->dmac_b->dma_handle);
	kfree(ecc->dmac_a);
	kfree(ecc->dmac_b);
	dma_pool_destroy(pool_a);
	dma_pool_destroy(pool_b);

	// Disable EDMA
	//edma_stop(ecc->dmac_a->dma_ch);
	//edma_free_slot(ecc->slot_20);

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
