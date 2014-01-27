
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

struct mem_addr {
	dma_addr_t	src_addr;
	dma_addr_t	dst_addr;
};

struct ebic_dmac {
	struct mem_addr		*kmem_addr;		/* Memory Address returned from dma_pool_alloc */
	dma_addr_t		dma_handle;		/* DMA Handle returned from dma_pool_alloc */
};

static struct dma_pool *pool;				/* Required for dma_pool_create */

static int __init dma_init(void) {

	struct ebic_dmac *dmac;
	struct mem_addr *kmem_addr;

	printk(KERN_INFO "Allocating Memory for DMA\n");

	dmac = kzalloc(sizeof(struct ebic_dmac), GFP_KERNEL);
	if (!dmac) {
		printk(KERN_ERR "%s: failed to alloc mem for struct ebic_dmac\n", __func__);
		return -ENOMEM;
	}

	/* Create a dma_pool */
	pool = dma_pool_create("EBIC-MEMTEST", NULL, DMA_POOL_SIZE, 16, 0);
	if (!pool) {
		printk(KERN_ERR "%s: failed to create pool\n", __func__);
		return -ENOMEM;
	}

	/* Allocate memory from a dma_pool */
	/* dma.c does a kzalloc() before running this function, check into this if there's issues */
	kmem_addr = dma_pool_alloc(pool, GFP_ATOMIC, &dmac->dma_handle);
	if (!kmem_addr) {
		printk(KERN_ERR "%s: failed to allocate memory for dma_pool\n", __func__);
		return -ENOMEM;
	}

	/* DEBUG: Prints values for own sanity */
	printk(KERN_INFO "dma_handle: %08x kmem_addr: %p\n", dmac->dma_handle, kmem_addr);

	/* Free memory from dma_pool */
	printk(KERN_INFO "Freeing all dma_pool memory\n");

	dma_pool_free(pool, &dmac->kmem_addr, dmac->dma_handle);
	kfree(dmac);
	dma_pool_destroy(pool);

	
	return 0;

}

static void __exit dma_exit(void) {

        printk(KERN_INFO "Exiting EBIC DMA Driver\n");

}

module_init(dma_init);		/* Runs when INSMOD or MODPROBE starts */
module_exit(dma_exit);		/* Runs when RMMOD or MODPROBE -R starts */
 
MODULE_AUTHOR("Ephemeron Labs Inc.");
MODULE_DESCRIPTION("Basic EDMA to Network Transfers");
MODULE_LICENSE("GPL");
