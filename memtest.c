
/*
 *
 * Using the DMA Pool API (Documentation/DMA-API-HOWTO.txt) to allocate memory for DMA transfers
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/dmapool.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/slab.h>

#define DMA_POOL_SIZE	4096

struct ebic_dmac {
	dma_addr_t		dma_handle;
};

/* Required for dma_pool_create() */
static struct dma_pool *pool;

static int __init dma_init(struct ebic_dmac *dmac) {

	printk(KERN_INFO "Allocating Memory for DMA\n");

	/* Create a dma_pool */
	pool = dma_pool_create("EBIC-MEMTEST", NULL, DMA_POOL_SIZE, 16, 0);
	if (!dma_pool) {
		printk(KERN_ERR "%s: failed to create pool\n", __func__);
		return -ENOMEM;
	}

	/* Allocate memory from a dma_pool */
	/* dma.c does a kzalloc() before running this function, check into this if there's issues */
	kmem_addr = dma_pool_alloc(pool, SLAB_ATOMIC, &dmac->dma_handle);
	if (!kmem_addr) {
		printk(KERN_ERR "%s: failed to allocate memory for dma_pool\n", __func__);
		ret = -ENOMEM;
		goto err_buff;
	}

	pr_debug("%s: kmem_addr: (%p, %08x) dma_handle: %d\n", __func__, kmem_addr, (u32)dmac->dma_handle);

	return 0;

}

static int __init dma_free(struct ebic_dmac *dmac) {

	printk(KERN_INFO "Freeing all dma_pool memory\n");

	dma_pool_free(pool, kmem_addr, dmac->dma_handle);
	dma_pool_destroy(pool);

}

module_init(dma_init);
module_exit(dma_free);

MODULE_AUTHOR("Ephemeron Labs Inc.");
MODULE_DESCRIPTION("EBIC Memory Test");
MODULE_LICENSE("GPL");

