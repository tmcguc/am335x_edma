/*
 * An attempt to setup DMA transfer with Ping/Pong Buffering interfacing with EDMA.c
 *
 *
 * Step 1. Initiate DMA Channel
 * 	a. Choose Type: DMA
 * 	b. Channel Mapping: Program DCHMAP with parameter set number to which the channel maps
 * 	c. Shadow Region: N/A
 * 	d. Triggering: Manual (Test) External (Prod)
 * 	e. Queue: Setup DMAQNUM to map the event to the respective event queue
 * Step 2. Parameter Set Setup
 * 	a. Program the PaRAM set number associated with the channel
 * 		Ping/Pong Buffering PaRAM Setup
 * 		Instead of one parameter set, there are two; one for transferring data to/from the ping buffers and one for transferring data to/from the pong buffers.
 * 		Note: https://github.com/tmcguc/am335x_pru_package/wiki/EDMA3cc-configuration
 * Step 3. Interrupt Setup
 * 	a. Enable the interrupt in the IER/IERH by writing into IESR/IESRH
 * 	b. Ensure that EDMA3CC completion interrupt (global) is enabled properly in the device interrupt controller
 * 	d. Setup the interrupt controller properly to receive the expected EDMA3 interrupt
 * Step 4. Initiate Transfer
 *	a. Event Trigger Source
 * 	 i. External
 * 	 iii. Manually triggered transfers will be initiated by writes to the Event Set Registers (ESR/ESRH)
 * Step 5. Wait for Completion
 * 	a. Interrupts Enabled: EDMA3CC will generate a completion interrupt to the CPU whenever transfer completion results in setting the corresponding bits in the interrupt pending register (IPR/IPRH)
 * 		The set bits must be cleared in the (IPR/IPRH) by writing to corresponding bit in (ICR/ICRH)
 *
 * 	Notes: https://github.com/tmcguc/am335x_pru_package/wiki/Ping-Pong-Buffer-Driver---Notes 
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

#define EDMA_ESR	0x1010
#define PINGCHAN	20
#define PONGCHAN	64

/*
 * EDMA3 Configuration
 * 	So edma.c takes care of most of the functions that the TRM suggests are needed for Setting up a Transfer. 
 * 	davinci-pcm.c leverages these functions along with a couple well-defined structs that allow it to setup
 * 	DMA transfers correctly.
 *
 * 	Our goal is to leverage these same functions davinci-pcm.c does and call the necessary functions in edma.c
 * 	to get DMA running.
 *
 * 	edma.h has a good list of the needed functions (Setters, Getters, Allocaters and Frees)
 *
 * 	edma.h also has struct edmacc_param which lays out the fields for PaRAM slots
 *
 * 	davinci-pcm.c has two parameter slots they call asp_params and ram_params (both edmacc_param structs) 
 * 
 */ 

static struct trans_dma_params {

	int				dma_ch;
	int				dma_link[2];
	struct edmacc_param		pingtrans;
	struct edmacc_param		pongtrans;

};

/*
 *	The following syntax is C99 specific, using the dot (.) allows me to set values for a edmacc_param struct defind in edma.h
 */
static struct edmacc_param pingtrans = {
	.opt = 0,							/* edmacc_params.opt offsets available from edma.h */
	.src = 0,
	.a_b_cnt = 0,
	.dst = 0,
	.src_dst_bidx = 0,
	.link_bcntrld = 0,
	.src_dst_cidx = 0,
	.ccnt = 0,
};


static struct edmacc_param pongtrans = {
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

static int setup_edma(struct trans_dma_params *tp) {

	int dma_channel;
	int ret;

	// Triggering - Manually-triggered Transfer Request: Writinig a 1 to the corresponding bit in the Event Set Register
	//edma_write(ctlr, EDMA_ESR, (1 << 17));

	/* Allocate Master Channel */
	ret = edma_alloc_channel(dma_channel, dma_callback, tp, EVENTQ_0);
	if (ret < 0) {
		printk(KERN_INFO "allocating channel for DMA failed\n");
		return ret;
	}
	tp->dma_ch = ret;
	printk(KERN_INFO "dma_ch=%d\n", tp->dma_ch);

	/* Allocate Link channels */
	ret = edma_alloc_slot(EDMA_CTLR(tp->dma_ch), EDMA_SLOT_ANY);
	if (ret < 0) {
		printk(KERN_INFO "allocating slot for DMA failed\n");
		return ret;
	}
	tp->dma_link[0] = ret;
	printk(KERN_INFO "dma_link[0]=%d\n", tp->dma_link[0]);

	// Allocate slot before writing, x should be the returned INT from edma_alloc_slot (REF: L447 davinci-pcm.c)
	//edma_write_slot(x, &pingtrans);
	
	return 0;
}
