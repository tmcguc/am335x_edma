/*
 * mighty_dma.c
 *
 *   Modified from the TI EDMA Sample Application
 *   Changes specific to the Mighty EBIC Scan Controller
 *   this is not a general-purpose EDMA driver
 * 
 *   Andrew Righter <q@crypto.com>
 *	for Ephemeron-Labs, Inc.
 *
 *   TODO - Enable users to easily switch between ASYNC and ABSYNC modes (edma_set_transfer_parameters)
 *   TODO - Have users dynamically set OPT values (we're intending to do it from IOCTL in Userland for our own use) 
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

#include <linux/platform_data/edma.h>
#include <linux/memory.h>

//#include <asm/hardware/edma.h>
//#include <mach/memory.h>

//#undef EDMA3_DEBUG
#define EDMA3_DEBUG

//#undef EBIC
#define EBIC

/* Which Sync Mode Do you Require? */
/* TODO */

/* How many bytes do you wish to transfer, in total? */
#ifdef EBIC
#define MAX_DMA_TRANSFER_IN_BYTES   (32)	/* EBIC test values (ACNT*BCNT*CCNT) */
#else
#define MAX_DMA_TRANSFER_IN_BYTES   (32768)     /* Original TI EDMA Sample Values */
#endif

/* useful defines for debug output */
#ifdef EDMA3_DEBUG
#define DMA_PRINTK(ARGS...)  printk(KERN_INFO "<%s>: ",__FUNCTION__);printk(ARGS)
#define DMA_FN_IN printk(KERN_INFO "[%s]: start\n", __FUNCTION__)
#define DMA_FN_OUT printk(KERN_INFO "[%s]: end\n",__FUNCTION__)
#else
#define DMA_PRINTK( x... )
#define DMA_FN_IN
#define DMA_FN_OUT
#endif

#define STATIC_SHIFT                3
#define TCINTEN_SHIFT               20
#define ITCINTEN_SHIFT              21
#define TCCHEN_SHIFT                22
#define ITCCHEN_SHIFT               23

static volatile int irqraised1 = 0;
static volatile int irqraised2 = 0;

int edma3_memtomemcpytest_dma_link(int acnt, int bcnt, int ccnt, int sync_mode, int event_queue);

dma_addr_t dmaphyssrc1 = 0;
dma_addr_t dmaphyssrc2 = 0;
dma_addr_t dmaphysdest1 = 0;
dma_addr_t dmaphysdest2 = 0;

char *dmabufsrc1 = NULL;
char *dmabufsrc2 = NULL;
char *dmabufdest1 = NULL;
char *dmabufdest2 = NULL;

/* EBIC values */
#ifdef EBIC
static int acnt = 4;		/* FIFO width (32-bit) */
static int bcnt = 8;		/* FIFO depth */
static int ccnt = 1;
#else
/* original values */
static int acnt = 512;
static int bcnt = 8;
static int ccnt = 8;
#endif

module_param(acnt, int, S_IRUGO);
module_param(bcnt, int, S_IRUGO);
module_param(ccnt, int, S_IRUGO);


static void callback1(unsigned lch, u16 ch_status, void *data)
{
	switch(ch_status) {
	case DMA_COMPLETE:
		irqraised1 = 1;
		/*DMA_PRINTK ("\n From Callback 1: Channel %d status is: %u\n",lch, ch_status);*/
		break;
	case DMA_CC_ERROR:
		irqraised1 = -1;
		DMA_PRINTK ("\nFrom Callback 1: DMA_CC_ERROR occured on Channel %d\n", lch);
		break;
	default:
		break;
	}
}

static int __init edma_test_init(void)
{
	int result = 0;
	int iterations = 0;
	#ifdef EBIC
	int numTCs = 2;
	#else	
	int numTCs = 4;
	#endif
	int modes = 2;
	int i,j;

	printk ("\nInitializing edma_test module\n");

	DMA_PRINTK ( "\nACNT=%d, BCNT=%d, CCNT=%d", acnt, bcnt, ccnt);

	/* allocate consistent memory for DMA
	 * dmaphyssrc1(handle)= device viewed address.
	 * dmabufsrc1 = CPU-viewed address
	 */

	dmabufsrc1 = dma_alloc_coherent (NULL, MAX_DMA_TRANSFER_IN_BYTES, &dmaphyssrc1, 0);
	DMA_PRINTK( "\nSRC1:\t%x", dmaphyssrc1);
	if (!dmabufsrc1) {
		DMA_PRINTK ("dma_alloc_coherent failed for dmaphyssrc1\n");
		return -ENOMEM;
	}

	dmabufdest1 = dma_alloc_coherent (NULL, MAX_DMA_TRANSFER_IN_BYTES, &dmaphysdest1, 0);
	DMA_PRINTK( "\nDST1:\t%x", dmaphysdest1);
	if (!dmabufdest1) {
		DMA_PRINTK("dma_alloc_coherent failed for dmaphysdest1\n");
		dma_free_coherent(NULL, MAX_DMA_TRANSFER_IN_BYTES, dmabufsrc1, dmaphyssrc1);
		return -ENOMEM;
	}

	dmabufsrc2 = dma_alloc_coherent (NULL, MAX_DMA_TRANSFER_IN_BYTES, &dmaphyssrc2, 0);
	DMA_PRINTK( "\nSRC2:\t%x", dmaphyssrc2);
	if (!dmabufsrc2) {
		DMA_PRINTK ("dma_alloc_coherent failed for dmaphyssrc2\n");

		dma_free_coherent(NULL, MAX_DMA_TRANSFER_IN_BYTES, dmabufsrc1, dmaphyssrc1);
		dma_free_coherent(NULL, MAX_DMA_TRANSFER_IN_BYTES, dmabufdest1, dmaphysdest1);
		return -ENOMEM;
	}

	dmabufdest2 = dma_alloc_coherent (NULL, MAX_DMA_TRANSFER_IN_BYTES, &dmaphysdest2, 0);
	DMA_PRINTK( "\nDST2:\t%x", dmaphysdest2);
	if (!dmabufdest2) {
		DMA_PRINTK ("dma_alloc_coherent failed for dmaphysdest2\n");

		dma_free_coherent(NULL, MAX_DMA_TRANSFER_IN_BYTES, dmabufsrc1, dmaphyssrc1);
		dma_free_coherent(NULL, MAX_DMA_TRANSFER_IN_BYTES, dmabufdest1, dmaphysdest1);
		dma_free_coherent(NULL, MAX_DMA_TRANSFER_IN_BYTES, dmabufsrc2, dmaphyssrc2);
		return -ENOMEM;
	}

	/* Test Routine */
	for (iterations = 0 ; iterations < 10 ; iterations++) {
		DMA_PRINTK ("Iteration = %d\n", iterations);

		for (j = 0 ; j < numTCs ; j++) { //TC
			DMA_PRINTK ("TC = %d\n", j);

			for (i = 0 ; i < modes ; i++) {	//sync_mode
				DMA_PRINTK ("Mode = %d\n", i);

				if (0 == result) {
					DMA_PRINTK ("Starting edma3_memtomemcpytest_dma_link\n");
					result = edma3_memtomemcpytest_dma_link(acnt, bcnt, ccnt, i, j);
					if (0 == result) {
						printk("edma3_memtomemcpytest_dma_link passed\n");
					} else {
						printk("edma3_memtomemcpytest_dma_link failed\n");
					}
				}

			}
		}
	}

	return result;
}


void edma_test_exit(void)
{
	dma_free_coherent(NULL, MAX_DMA_TRANSFER_IN_BYTES, dmabufsrc1, dmaphyssrc1);
	dma_free_coherent(NULL, MAX_DMA_TRANSFER_IN_BYTES, dmabufdest1, dmaphysdest1);

	dma_free_coherent(NULL, MAX_DMA_TRANSFER_IN_BYTES, dmabufsrc2, dmaphyssrc2);
	dma_free_coherent(NULL, MAX_DMA_TRANSFER_IN_BYTES, dmabufdest2, dmaphysdest2);

	printk ("\nExiting edma_test module\n");
}

/* 2 DMA Channels Linked, Mem-2-Mem Copy, ASYNC Mode, INCR Mode */
int edma3_memtomemcpytest_dma_link(int acnt, int bcnt, int ccnt, int sync_mode, int event_queue)
{
	int result = 0;
	unsigned int dma_ch1 = 0;
	unsigned int dma_ch2 = 0;
	int i;
	int count = 0;
	unsigned int Istestpassed1 = 0u;
	unsigned int Istestpassed2 = 0u;
	unsigned int numenabled = 0;
	unsigned int BRCnt = 0;
	int srcbidx = 0;
	int desbidx = 0;
	int srccidx = 0;
	int descidx = 0;
	struct edmacc_param param_set;

	/* Initalize source and destination buffers */
	for (count = 0u; count < (acnt*bcnt*ccnt); count++) {
		dmabufsrc1[count] = 'A' + (count % 26);
		dmabufdest1[count] = 0;

		dmabufsrc2[count] = 'A' + (count % 26);
		dmabufdest2[count] = 0;
	}

	/* Set B count reload as B count. */
	BRCnt = bcnt;

	/* Setting up the SRC/DES Index */
	srcbidx = acnt;
	desbidx = acnt;

	/* A Sync Transfer Mode */
	srccidx = acnt;
	descidx = acnt;

	result = edma_alloc_channel (EDMA_CHANNEL_ANY, callback1, NULL, event_queue);

	if (result < 0) {
		DMA_PRINTK ("edma3_memtomemcpytest_dma_link::edma_alloc_channel failed for dma_ch1, error:%d\n", result);
		return result;
	}

	dma_ch1 = result;
	edma_set_src (dma_ch1, (unsigned long)(dmaphyssrc1), INCR, W8BIT);
	edma_set_dest (dma_ch1, (unsigned long)(dmaphysdest1), INCR, W8BIT);
	edma_set_src_index (dma_ch1, srcbidx, srccidx);
	edma_set_dest_index (dma_ch1, desbidx, descidx);
	/* TODO - Change to ABSYNC */
	edma_set_transfer_params (dma_ch1, acnt, bcnt, ccnt, BRCnt, ASYNC);

	/* Enable the Interrupts on Channel 1 */
	edma_read_slot (dma_ch1, &param_set);
	param_set.opt |= (1 << ITCINTEN_SHIFT);
	param_set.opt |= (1 << TCINTEN_SHIFT);
	param_set.opt |= EDMA_TCC(EDMA_CHAN_SLOT(dma_ch1));
	edma_write_slot(dma_ch1, &param_set);

	/* Request a Link Channel */
	result = edma_alloc_slot (0, EDMA_SLOT_ANY);

	if (result < 0) {
		DMA_PRINTK ("\nedma3_memtomemcpytest_dma_link::edma_alloc_slot failed for dma_ch2, error:%d\n", result);
		return result;
	}

	dma_ch2 = result;
	edma_set_src (dma_ch2, (unsigned long)(dmaphyssrc2), INCR, W8BIT);
	edma_set_dest (dma_ch2, (unsigned long)(dmaphysdest2), INCR, W8BIT);
	edma_set_src_index (dma_ch2, srcbidx, srccidx);
	edma_set_dest_index (dma_ch2, desbidx, descidx);
	/* TODO - Change to ABSYNC */
	edma_set_transfer_params (dma_ch2, acnt, bcnt, ccnt, BRCnt, ASYNC);

	/* Enable the Interrupts on Channel 2 */
	edma_read_slot (dma_ch2, &param_set);
	param_set.opt |= (1 << ITCINTEN_SHIFT);
	param_set.opt |= (1 << TCINTEN_SHIFT);
	param_set.opt |= EDMA_TCC(EDMA_CHAN_SLOT(dma_ch1));
	edma_write_slot(dma_ch2, &param_set);

	/* Link both the channels */
	edma_link(dma_ch1, dma_ch2);

	numenabled = bcnt * ccnt;

	for (i = 0; i < numenabled; i++) {
		irqraised1 = 0;

		/*
		 * Now enable the transfer as many times as calculated above.
		 */
		result = edma_start(dma_ch1);
		if (result != 0) {
			DMA_PRINTK ("edma3_memtomemcpytest_dma_link: davinci_start_dma failed \n");
			break;
		}

		/* Wait for the Completion ISR. */
		while (irqraised1 == 0u);

		/* Check the status of the completed transfer */
		if (irqraised1 < 0) {
			/* Some error occured, break from the FOR loop. */
			DMA_PRINTK ("edma3_memtomemcpytest_dma_link: Event Miss Occured!!!\n");
			break;
		}
	}

	if (result == 0) {
		for (i = 0; i < numenabled; i++) {
			irqraised1 = 0;

			/*
			 * Now enable the transfer as many times as calculated above
			 * on the LINK channel.
			 */
			result = edma_start(dma_ch1);
			if (result != 0) {
				DMA_PRINTK ("\nedma3_memtomemcpytest_dma_link: davinci_start_dma failed \n");
				break;
			}

			/* Wait for the Completion ISR. */
			while (irqraised1 == 0u);

			/* Check the status of the completed transfer */
			if (irqraised1 < 0) {
				/* Some error occured, break from the FOR loop. */
				DMA_PRINTK ("edma3_memtomemcpytest_dma_link: "
						"Event Miss Occured!!!\n");
				break;
			}
		}
	}

	if (0 == result) {
		for (i = 0; i < (acnt*bcnt*ccnt); i++) {
			if (dmabufsrc1[i] != dmabufdest1[i]) {
				DMA_PRINTK ("\nedma3_memtomemcpytest_dma_link(1): Data "
						"write-read matching failed at = %u\n",i);
				Istestpassed1 = 0u;
				break;
			}
		}
		if (i == (acnt*bcnt*ccnt)) {
			Istestpassed1 = 1u;
		}

		for (i = 0; i < (acnt*bcnt*ccnt); i++) {
			if (dmabufsrc2[i] != dmabufdest2[i]) {
				DMA_PRINTK ("\nedma3_memtomemcpytest_dma_link(2): Data "
						"write-read matching failed at = %u\n",i);
				Istestpassed2 = 0u;
				break;
			}
		}
		if (i == (acnt*bcnt*ccnt)) {
			Istestpassed2 = 1u;
		}

		edma_stop(dma_ch1);
		edma_free_channel(dma_ch1);

		edma_stop(dma_ch2);
		edma_free_slot(dma_ch2);
		edma_free_channel(dma_ch2);
	}

	if ((Istestpassed1 == 1u) && (Istestpassed2 == 1u)) {
		DMA_PRINTK ("\nedma3_memtomemcpytest_dma_link: EDMA Data Transfer Successfull\n");
	} else {
		DMA_PRINTK ("\nedma3_memtomemcpytest_dma_link: EDMA Data Transfer Failed\n");
	}

	return result;
}

module_init(edma_test_init);
module_exit(edma_test_exit);

MODULE_AUTHOR("Ephemeron Labs, Inc.");
MODULE_LICENSE("GPL");
