/*
 * An attempt to setup DMA transfer with Ping/Pong Buffering interfacing with EDMA.c
 * 	Notes: https://github.com/tmcguc/am335x_pru_package/wiki/Ping-Pong-Buffer-Driver---Notes 
 *
 *	platform device references:
 *		http://lwn.net/Articles/448502/
 *		http://lwn.net/Articles/448499/
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/kernel.h>
#include <linux/genalloc.h>
//#include <linux/platform_data/edma.h>

#include <asm/dma.h>

#include "edma.h"

#define PINGCHAN	20
#define PONGCHAN	64

/*
 * EDMA3 Configuration
 * 	So edma.c takes care of most of the functions that the TRM suggests are needed for Setting up a Transfer. 
 * 	davinci-pcm.c leverages these functions along with a couple well-defined structs that allow it to setup
 * 	DMA transfers correctly.
 */ 

static struct ebic_dev {
	spinlock_t			lock;
	int				dma_ch;
	int				dma_link[2];
	int				slot;
	struct edmacc_param		pingset;
	struct edmacc_param		pongset;

};

/*
 *	The following syntax is C99 specific, using the dot (.) allows me to set values for a edmacc_param struct defind in edma.h
 */
static struct edmacc_param pingset = {
	.opt = 0,							/* edmacc_params.opt offsets available from edma.h */
	.src = 0,
	.a_b_cnt = 0,
	.dst = 0,
	.src_dst_bidx = 0,
	.link_bcntrld = 0,
	.src_dst_cidx = 0,
	.ccnt = 0,
};


static struct edmacc_param pongset = {
	.opt = 0,							/* edmacc_params.opt offsets available from edma.h */
	.src = 0,
	.a_b_cnt = 0,
	.dst = 0,
	.src_dst_bidx = 0,
	.link_bcntrld = 0,
	.src_dst_cidx = 0,
	.ccnt = 0,
};

static void dma_callback(unsigned link, u16 ch_status, void *data) {

	printk(KERN_INFO "EDMA transfer test: link=%d, status=0x%x\n", link, ch_status);

	if (unlikely(ch_status != DMA_COMPLETE))
		return;
}

static int edma_probe(struct ebic_dev *edev) {

	int dma_channel = PINGCHAN;
	int ctlr;
	int ret;

	printk("entering edma_probe\n");

	/* 
	* Allocate CHANNELS
	*
	*  Here's the skinny: Allocating dma_channel (master), exception handler if RET < 0 then allocation failed
	*	then print to kernel debug (dmesg | tail) then I'm setting RET equal to the struct ebic_dev 
	* 	field dma_chan for later use.
	*
	*/
	ret = edma_alloc_channel(dma_channel, dma_callback, edev, EVENTQ_0);
	if (ret < 0) {
		printk(KERN_INFO "allocating channel for DMA failed\n");
		return ret;
	}
	edev->dma_ch = ret;
	printk(KERN_INFO "ret=%d, dma_ch=%d\n", ret, edev->dma_ch);

	/* 
	* Allocate SLOTS
	*
	*  Here's the skinny: For testing I just need a single secondary channel to transfer data and test this shit
	*
	*/
	ret = edma_alloc_slot(EDMA_CTLR(edev->dma_ch), EDMA_SLOT_ANY);
	if (ret < 0) {
		printk(KERN_INFO "allocating slot for DMA failed\n");
		return ret;
	}
	edev->dma_link[0] = ret;
	printk(KERN_INFO "ret=%d, dma_link[0]=%d\n", ret, edev->dma_link[0]);
	
	// Allocate slot before writing, first arg should be the returned INT from edma_alloc_slot (REF: L447 davinci-pcm.c)
	edma_write_slot(ret, &edev->pingset);
	edev->slot = ret;

	// Enable DMA
	ret = edma_start(edev->dma_ch);
	if (ret != 0) {
		printk(KERN_INFO "edma_start failed\n");
		return ret;
	}
	
	return 0;
}

/*
*	we need a platform device, pointing to the platform_driver to register correctly with kernel
*/
static struct platform_device *pdev0;

static int edma_remove(struct ebic_dev *edev) {

	edma_free_slot(edev->slot); 

	return 0;
}

/* 
*	platform_device.h - required for registering/unregistering modules as platform drivers
*/
static struct platform_driver edma_driver = {

	.probe		= (struct ebic_dev *)edma_probe,
	.remove		= (struct ebic_dev *)edma_remove,
	.driver = {
		.name = "edma-ebic",
		.owner = THIS_MODULE,
	},
};

static const struct platform_device_info edma_dev_info0 = {
	.name = "edma-ebic",
	.id = 0,
	.dma_mask = DMA_BIT_MASK(32),
};

static int __init edma_init(void) {

	int ret = platform_driver_register(&edma_driver);
	
	if (ret == 0) {
		pdev0 = platform_device_register_full(&edma_dev_info0);
		printk(KERN_INFO "Platform Device Registered!\n");
		/*if (IS_ERR(pdev0)) {
			platform_driver_unregister(&edma_driver);
			ret = PTR_ERR(pdev0);
			goto out;
		}*/
	}

out:
	return ret;
}

/*
*	module_init must go before module_exit is declared otherwise circular depends issues with .remove = gtfo
*/
module_init(edma_init);

static void __exit gtfo(struct ebic_dev *edev) {

	printk("exiting transfer driver\n");
	printk(KERN_INFO "edev->dma_chan=%d\n", edev->dma_ch);
	// Disable EDMA
	//edma_stop(edev->dma_ch);

	platform_device_unregister(pdev0);
	platform_driver_unregister(&edma_driver);

}

module_exit(gtfo);

MODULE_AUTHOR("Ephemeron Labs Inc.");
MODULE_DESCRIPTION("Basic EDMA to Network Transfers");
MODULE_LICENSE("GPL");
