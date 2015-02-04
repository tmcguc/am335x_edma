/*
 *
 *  Andrew Righter <q@crypto.com>
 *  Ephemeron Labs, Inc.
*/ 

#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include <linux/moduleparam.h>
#include <linux/sysctl.h>
#include <linux/mm.h>
#include <linux/dma-mapping.h>

#include <mach/hardware.h>
#include <mach/irqs.h>

/* Replaced the following header files as the ones TI used did not exist on my system */

/* Replaced ... */
//#include <mach/memory.h>

/* ... with */
#include <linux/memory.h>

/* Replaced ... */
//#include <asm/hardware/edma.h>

/* ... with */
#include <linux/platform_data/edma.h>

/*#undef EDMA3_DEBUG*/
#define EDMA3_DEBUG

#ifdef EDMA3_DEBUG
#define DMA_PRINTK(ARGS...)  printk(KERN_INFO "<%s>: ",__FUNCTION__);printk(ARGS)
#define DMA_FN_IN printk(KERN_INFO "[%s]: start\n", __FUNCTION__)
#define DMA_FN_OUT printk(KERN_INFO "[%s]: end\n",__FUNCTION__)
#else
#define DMA_PRINTK( x... )
#define DMA_FN_IN
#define DMA_FN_OUT
#endif

#define MAX_DMA_TRANSFER_IN_BYTES   (32768)
#define STATIC_SHIFT                3
#define TCINTEN_SHIFT               20
#define ITCINTEN_SHIFT              21
#define TCCHEN_SHIFT                22
#define ITCCHEN_SHIFT               23

static volatile int irqraised1 = 0;
static volatile int irqraised2 = 0;
/*
int edma3_memtomemcpytest_dma(int acnt, int bcnt, int ccnt, int sync_mode, int event_queue);
int edma3_memtomemcpytest_dma_link(int acnt, int bcnt, int ccnt, int sync_mode, int event_queue);
int edma3_memtomemcpytest_dma_chain(int acnt, int bcnt, int ccnt, int sync_mode, int event_queue);
*/
dma_addr_t dmaphyssrc1 = 0;
dma_addr_t dmaphyssrc2 = 0;
dma_addr_t dmaphysdest1 = 0;
dma_addr_t dmaphysdest2 = 0;

char *dmabufsrc1 = NULL;
char *dmabufsrc2 = NULL;
char *dmabufdest1 = NULL;
char *dmabufdest2 = NULL;

static int acnt = 512;
static int bcnt = 8;
static int ccnt = 8;

module_param(acnt, int, S_IRUGO);
module_param(bcnt, int, S_IRUGO);
module_param(ccnt, int, S_IRUGO);

static int __init edma_test_init(void) {

	int result = 0;
	int iterations = 0;
	int numTCs = 4;
	int modes = 2;
	int i,j;

	printk ("\nInitializing EDMA module.\n");

	DMA_PRINTK ("\nACNT=%d, BCNT=%d, CCNT=%d", acnt, bcnt, ccnt);

	/* allocate consistent memory for DMA
	 * dmaphyssrc1(handle) = device viewed addr
	 * dmabufsrc1 = CPU viewed addr
	 */

	/* src1 */
	dmabufsrc1 = dma_alloc_coherent(NULL, MAX_DMA_TRANSFER_IN_BYTES, &dmaphyssrc1, 0);

	DMA_PRINTK("\nSRC1:\t%x", dmaphyssrc1);
	if (!dmabufsrc1) {
		DMA_PRINTK("dma_alloc_coherent failed for dmaphyssrc1\n");
		return -ENOMEM;
	}
	/* dst1 */
	dmabufdest1 = dma_alloc_coherent(NULL, MAX_DMA_TRANSFER_IN_BYTES, &dmaphysdest1, 0);

	DMA_PRINTK("\nDST1:\t%x", dmaphysdest1);
	if (!dmabufdest1) {
		DMA_PRINTK("dma_alloc_coherent failed for dmaphysdest1\n");
		dma_free_coherent(NULL, MAX_DMA_TRANSFER_IN_BYTES, dmabufsrc1, dmaphyssrc1);
		return -ENOMEM;
	}

	/* add in src2 dest2 */

	return 0;
}

void edma_test_exit(void) {

	DMA_FN_OUT;

}

module_init(edma_test_init);		/* Start the Module! */
module_exit(edma_test_exit);		/* End the Module! */

MODULE_AUTHOR("Texas Instruments");
MODULE_LICENSE("GPL");
