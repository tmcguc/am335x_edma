
/*
 *
 * Using the DMA Pool API (Documentation/DMA-API-HOWTO.txt) to allocate memory for DMA transfers
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/dmapool.h>		/* DMA Pool API */
#include <linux/dma-mapping.h>		/* dma_addr_t type */
#include <linux/genalloc.h>		/* DMA allocation stuff */
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>

#include <linux/platform_device.h>	/* Register/Unregister Platform Drivers - Do we need this? */

#define DMA_POOL_SIZE	4096

/* DMA memory addresses */
struct mem_addr {
	u32	src_addr;			/* XFER TEST: pool_a memory addr */
	u32	dst_addr;			/* XFER TEST: pool_b memory addr */
};

/* EBIC DMA Channel Structure */
struct ebic_dmac {
	struct mem_addr		*kmem_addr;		/* Memory Address returned from dma_pool_alloc */
	dma_addr_t		dma_handle;		/* DMA Handle set by dma_pool_alloc */
};

static struct dma_pool *pool_a;				/* Required for dma_pool_create */
static struct dma_pool *pool_b;

static int __init dma_init(void) {

	struct ebic_dmac *dmac_a;
	struct ebic_dmac *dmac_b;

	struct mem_addr *kmem_addr_a;
	struct mem_addr *kmem_addr_b;

	struct device *dev_a;			/* Where are these set? */
	struct device *dev_b;
	
	printk(KERN_INFO "Allocating Memory for DMA\n");

	/* Allocate mem for dmac structs */
	dmac_a = kzalloc(sizeof(struct ebic_dmac), GFP_KERNEL);
	dmac_b = kzalloc(sizeof(struct ebic_dmac), GFP_KERNEL);
	
	if (!dmac_a || dmac_b) {
		printk(KERN_ERR "%s: failed to alloc mem for struct ebic_dmac\n", __func__);
		return -ENOMEM;
	}


	/* Create a dma_pool */
	pool_a = dma_pool_create("pool_a memtest", NULL, DMA_POOL_SIZE, 16, 0);
	pool_b = dma_pool_create("pool_b memtest", NULL, DMA_POOL_SIZE, 16, 0);
	
	if (!pool_a || pool_b) {
		printk(KERN_ERR "%s: failed to create pool\n", __func__);
		return -ENOMEM;
	}
	

	/* Allocate memory from a dma_pool */
	/* dma.c does a kzalloc() before running this function, check into this if there's issues */
	kmem_addr_a = dma_pool_alloc(pool_a, GFP_ATOMIC, &dmac_a->dma_handle);
	kmem_addr_b = dma_pool_alloc(pool_b, GFP_ATOMIC, &dmac_b->dma_handle);

	if (!kmem_addr_a || kmem_addr_b) {
		printk(KERN_ERR "%s: failed to allocate memory for dma_pool\n", __func__);
		return -ENOMEM;
	}


	/* DEBUG: Prints values for own sanity */
	printk(KERN_INFO "dma_handle_a: 0x%08x dma_handle_b: 0x%08x", dmac_a->dma_handle, dmac_b->dma_handle);
	printk(KERN_INFO "kmem_addr_a:%08x, kmem_addr_b:%08x", &kmem_addr_a, &kmem_addr_b);

	//printk(KERN_INFO "kmem_addr_a->src_addr: %08x, kmem_addr_b->src_addr: %08x", kmem_addr_a->src_addr, kmem_addr_b->dst_addr);
	//printk(KERN_INFO "kmem_addr_a->dst_addr: %08x, kmem_addr_b->dst_addr: %08x", kmem_addr_a->dst_addr, kmem_addr_b->dst_addr);

	return 0;

}

static void __exit dma_exit(void) {

	/* Free memory from dma_pool */
	printk(KERN_INFO "Freeing all dma_pool memory\n");

	dma_pool_free(pool_a, &kmem_addr_a, dmac_a->dma_handle);
	dma_pool_free(pool_b, &kmem_addr_b, dmac_b->dma_handle);
	kfree(dmac_a);
	kfree(dmac_b);
	dma_pool_destroy(pool_a);
	dma_pool_destroy(pool_b);

        printk(KERN_INFO "Exiting EBIC DMA Driver\n");

}

module_init(dma_init);		/* Runs when INSMOD or MODPROBE starts */
module_exit(dma_exit);		/* Runs when RMMOD or MODPROBE -R starts */
 
MODULE_AUTHOR("Ephemeron Labs Inc.");
MODULE_DESCRIPTION("Basic EDMA to Network Transfers");
MODULE_LICENSE("GPL");
