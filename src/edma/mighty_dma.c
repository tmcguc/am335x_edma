/*
 * mighty_dma.c
 *
 *   Modified from the TI EDMA Sample Application
 *   Changes specific to the Mighty EBIC Scan Controller
 *   this is not a general-purpose EDMA driver
 * 
 *   Andrew Righter <q@crypto.com>
 *   Terrence McGuckin <terrence@ephemeron-labs.com>
 *	for Ephemeron-Labs, Inc.
 *
 *   TODO - Enable users to easily switch between ASYNC and ABSYNC modes (edma_set_transfer_parameters)
 *   TODO - Have users dynamically set OPT values (we're intending to do it from IOCTL in Userland for our own use) 
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include <linux/moduleparam.h>
#include <linux/sysctl.h>
#include <linux/mm.h>
#include <linux/dma-mapping.h>
#include <linux/cdev.h>  // Added by TM

#include <mach/hardware.h>
#include <mach/irqs.h>

#include <linux/platform_data/edma.h>
#include <linux/memory.h>

//#include <asm/hardware/edma.h>
//#include <mach/memory.h>

#define EBIC_DEV_NAME "mighty_ebic"

//#undef EDMA3_DEBUG
#define EDMA3_DEBUG

//#undef EBIC
#define EBIC

/* Which Sync Mode Do you Require? */
/* TODO - Look at TMC changes in ABsync branch to understand A/AB TM diff */

/* How many bytes do you wish to transfer, in total? */
#ifdef EBIC
#define MAX_DMA_TRANSFER_IN_BYTES   (32768)	/* Don't need two of these defines in the future */
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
#define MAJOR_NUMBER_MIGHTY	    0

static volatile int irqraised1 = 0;
static volatile int irqraised2 = 0;

int edma3_memtomemcpytest_dma_link(int acnt, int bcnt, int ccnt, int sync_mode, int event_queue);
int edma3_fifotomemcpytest_dma_link(int acnt, int bcnt, int ccnt, int sync_mode, int event_queue);

/*Added by TM variables for keeping tracking of wether we are on  ping and pong and status of transfer    */
static int stop_ping_pong(int *ch, int *slot1, int *slot2);
int ch = 17;
int slot1;
int slot2;
int *ch_ptr = &ch;
int *slot1_ptr = &slot1;
int *slot2_ptr = &slot2;

/*Transfer counter*/
unsigned int transfer_counter = 0;
int ping = 1;
int ping_counter = 0;
int pong_counter = 0;



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
static int ccnt = 2;
#else
/* original values */
static int acnt = 512;
static int bcnt = 8;
static int ccnt = 8;
#endif


//This is outdated look at cdev.h for new way of doing this TM
static int major;			/* Major Revision for Char Device, Returned by register_chrdev */
struct file_operations mighty_fops;	/* File Operations to communicate with userland */

module_param(acnt, int, S_IRUGO);	/* Module Parameters allow you to pass them as arguments during insmod */
module_param(bcnt, int, S_IRUGO);	/* EX: insmod ./mighty_ebic.ko acnt=4, bcnt=8, ccnt=1 */
module_param(ccnt, int, S_IRUGO);


//Stuff to register char device added TM
dev_t mighty_dev;
unsigned int mighty_first_minor = 0;
unsigned int mighty_count = 1;
struct cdev cdev;
struct cdev *mighty_cdev = &cdev;






/* Begin File Operations for Communication with Userland */
static int ebic_open(struct inode *inode, struct file *file)
{
	DMA_PRINTK("successful\n");
		return 0;
}

static int ebic_release(struct inode *inode, struct file *file)
{
	DMA_PRINTK("successful\n");
	return 0;
}

static ssize_t ebic_read(struct file *file, char *buf, size_t count, loff_t *ptr)
{
	DMA_PRINTK("returning zero bytes\n");
	return 0;
}

static ssize_t ebic_write(struct file *file, const char *buf, size_t count, loff_t * ppos)
{
	DMA_PRINTK("accepting zero bytes\n");
	return 0;
}

static long ebic_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	printk("cmd=%d, arg=%ld\n", cmd, arg);
	return 0;
}
/* End File Operations for Communication with Userland */

static int setup_mighty_dev(void){



	if (alloc_chrdev_region(&mighty_dev, mighty_first_minor, 1, "mighty_dma") <
	    0) {
		printk(KERN_ERR "mighty: unable to find free device numbers\n");
		return -EIO;
	}

	cdev_init(mighty_cdev, &mighty_fops);

	if (cdev_add(mighty_cdev, mighty_dev, 1) < 0) {
		printk(KERN_ERR "broken: unable to add a character device\n");
		unregister_chrdev_region(mighty_dev, mighty_count);
		return -EIO;
	}

	printk(KERN_INFO "Loaded the mighty driver: major = %d, minor = %d\n",
	       MAJOR(mighty_dev), MINOR(mighty_dev));

	return 0;
}












static void callback1(unsigned lch, u16 ch_status, void *data)
{
	switch(ch_status) {
	case DMA_COMPLETE:
		irqraised1 = 1;
		DMA_PRINTK ("\n From Callback 1: Channel %d status is: %u\n",lch, ch_status); //TM added this back in to understand when callback is called can 
		break;										
	case DMA_CC_ERROR:
		irqraised1 = -1;
		DMA_PRINTK ("\nFrom Callback 1: DMA_CC_ERROR occured on Channel %d\n", lch);
		break;
	default:
		break;
	}

}

static void callback_pingpong(unsigned lch, u16 ch_status, void *data)
{
	//TODO check the status of IPR registers may need to manually reset these!!
	switch(ch_status) {
	case DMA_COMPLETE:
		irqraised1 = 1;
		DMA_PRINTK ("\n From Callback PingPong: Channel %d status is: %u\n",lch, ch_status); 
		// TODO use callabck put data into proper buffer, incrment a counter etc...
		++transfer_counter;
		if(ping == 1){
			++ping_counter;
			DMA_PRINTK ("\nTransfer from Ping: ping_counter is %d transfer_counter is: %d", ping_counter, transfer_counter); 
			//TODO add in functionality to trnafer to mmap buffer notfiy userland
			ping = 0;
			break;
		}
		else if(ping == 0){
			++pong_counter;
			DMA_PRINTK ("\nTransfer from Pong: pong_counter is %d transfer_counter is: %d", pong_counter, transfer_counter);
			ping = 1;
			break;
		}	
		else	
			break;										
	case DMA_CC_ERROR:
		irqraised1 = -1;
		DMA_PRINTK ("\nFrom Callback PingPong : DMA_CC_ERROR occured on Channel %d\n", lch);
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
	int registered;

	printk ("\nInitializing edma_test module\n");

	registered = setup_mighty_dev();
	//major = register_chrdev(MAJOR_NUMBER_MIGHTY, EBIC_DEV_NAME, &ebic_fops);
	if (registered < 0) {
		printk("Error registering mighty_ebic device\n");
		goto ebic_fail;
	}

	printk("\nmighty_ebic: registered module successfully!\n");

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
	for (iterations = 0 ; iterations < 5 ; iterations++) {
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
	
	result = edma3_fifotomemcpytest_dma_link(acnt,bcnt,ccnt,1,1);
	//result = stop_ping_pong(&ch, &slot);	

	return result;
/* If register_chrdev fails: */
ebic_fail:
	return major;
}


void edma_test_exit(void)
{
	stop_ping_pong (&ch, &slot1, &slot2);

	dma_free_coherent(NULL, MAX_DMA_TRANSFER_IN_BYTES, dmabufsrc1, dmaphyssrc1);
	dma_free_coherent(NULL, MAX_DMA_TRANSFER_IN_BYTES, dmabufdest1, dmaphysdest1);

	dma_free_coherent(NULL, MAX_DMA_TRANSFER_IN_BYTES, dmabufsrc2, dmaphyssrc2);
	dma_free_coherent(NULL, MAX_DMA_TRANSFER_IN_BYTES, dmabufdest2, dmaphysdest2);

	printk("\nUnregistering Driver\n");

	//unregister_chrdev(major, "mighty_ebic");

	cdev_del(mighty_cdev);
	unregister_chrdev_region(mighty_dev, mighty_count);
	printk(KERN_INFO "Unloaded the mighty driver!\n");

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

		dmabufsrc2[count] = 'B' + (count % 26);
		dmabufdest2[count] = 0;
	}

	/* Set B count reload as B count. */
	BRCnt = bcnt;

	/* Setting up the SRC/DES Index */
	srcbidx = acnt;
	desbidx = acnt;

	/* A Sync Transfer Mode */
	//srccidx = acnt;
	//descidx = acnt;

	/* AB Sync Transfer Mode */
	srccidx = bcnt * acnt;
	descidx = bcnt * acnt;

	result = edma_alloc_channel (EDMA_CHANNEL_ANY, callback1, NULL, event_queue);

	if (result < 0) {
		DMA_PRINTK ("edma3_memtomemcpytest_dma_link::edma_alloc_channel failed for dma_ch1, error:%d\n", result);
		return result;
	}

	dma_ch1 = result;
	edma_set_src (dma_ch1, (unsigned long)(dmaphyssrc1), INCR, W32BIT);
	edma_set_dest (dma_ch1, (unsigned long)(dmaphysdest1), INCR, W32BIT);
	edma_set_src_index (dma_ch1, srcbidx, srccidx);
	edma_set_dest_index (dma_ch1, desbidx, descidx);
	edma_set_transfer_params (dma_ch1, acnt, bcnt, ccnt, BRCnt, ABSYNC);

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
	edma_set_src (dma_ch2, (unsigned long)(dmaphyssrc2), INCR, W32BIT);
	edma_set_dest (dma_ch2, (unsigned long)(dmaphysdest2), INCR, W32BIT);
	edma_set_src_index (dma_ch2, srcbidx, srccidx);
	edma_set_dest_index (dma_ch2, desbidx, descidx);
	edma_set_transfer_params (dma_ch2, acnt, bcnt, ccnt, BRCnt, ABSYNC);

	/* Enable the Interrupts on Channel 2 */
	edma_read_slot (dma_ch2, &param_set);
	param_set.opt |= (1 << ITCINTEN_SHIFT);
	param_set.opt |= (1 << TCINTEN_SHIFT);
	param_set.opt |= EDMA_TCC(EDMA_CHAN_SLOT(dma_ch1));
	edma_write_slot(dma_ch2, &param_set);

	/* Link both the channels */
	edma_link(dma_ch1, dma_ch2);

	//numenabled = bcnt * ccnt;	/* For A Sync Transfer Mode */
	numenabled = ccnt;		/* For AB Sync Transfer Mode */

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




/* 2 DMA Channels Linked to each other, FIFO-2-Mem Copy, ABSYNC Mode, FIFO Mode, Ping Pong buffering scheme */
/*TODO: get rid of sync mode it doesn't do anything in the tests */
/*TODO: pass channel and FIFO address to function*/
int edma3_fifotomemcpytest_dma_link(int acnt, int bcnt, int ccnt, int sync_mode, int event_queue) 
{
	int result = 0;
	unsigned int dma_ch1 = 0;
	//unsigned int dma_ch2 = 0;
	unsigned int dma_slot1 = 0;
	unsigned int dma_slot2 = 0;
	int count = 0;
	unsigned int BRCnt = 0;
	int srcbidx = 0;
	int desbidx = 0;
	int srccidx = 0;
	int descidx = 0;
	//int src_ch = 17;			//TODO: should be passed or is setup as DEFINE
	unsigned long spi_fifo = 0x480301a0;   //TODO: should paas this value or set up as DEFINE this is for channel 17 SPI0RX DMA aligned FIFO

	struct edmacc_param param_set;

	/* Initalize source and destination buffers */
	/*grabbing from FIFO no need to initialize with data*/
	for (count = 0u; count < (acnt*bcnt*ccnt); count++) {
		dmabufdest1[count] = 0;

		dmabufdest2[count] = 0;
	}


	/* Set B count reload as B count. */
	BRCnt = bcnt; //changed from bcnt may not affect anything

	/* Setting up the SRC/DES Index */
	srcbidx = 0;
	desbidx = acnt;


	/* AB Sync Transfer Mode */
	srccidx = 0;
	descidx = bcnt * acnt;

/* GRAB all the channels and slots we need for liniking ping pong buffers and circling them*/

	result = edma_alloc_channel (ch, callback_pingpong, NULL, event_queue);  //TODO: write our own callback function that adds more functionality
	if (result < 0) {
		DMA_PRINTK ("edma3_fifotomemcpytest_dma_link::edma_alloc_channel failed for dma_ch1, error:%d\n", result);
		return result;
	}
	dma_ch1 = result;
	*ch_ptr = result; //Make sure we keep the same info on the channel TM

 
	/* Request a Link Channel for slot1 */
	result = edma_alloc_slot (0, EDMA_SLOT_ANY);  //grab link channel
	if (result < 0) {
		DMA_PRINTK ("\nedma3_fifotomemcpytest_dma_link::edma_alloc_slot failed for dma_ch2, error:%d\n", result);
		return result;
	}
	dma_slot1 = result;
	*slot1_ptr = dma_slot1;



	/* Request a Link Channel for slot2*/
	result = edma_alloc_slot (0, EDMA_SLOT_ANY);  //grab link channel
	if (result < 0) {
		DMA_PRINTK ("\nedma3_fifotomemcpytest_dma_link::edma_alloc_slot failed for dma_ch2, error:%d\n", result);
		return result;
	}
	dma_slot2 = result;
	*slot2_ptr = dma_slot2;


	/* Link the channel and the two slots needed for continous operation*/
	edma_link(dma_ch1, dma_slot2);   // channel reloads param from slot2 goes to pong
	edma_link(dma_slot2, dma_slot1);   // param in slot2 will reload from slot1
	edma_link(dma_slot1, dma_slot2);  // slot1 will reload from slot2


	edma_set_src (dma_ch1, spi_fifo, FIFO, W32BIT); // need to put in addres of FIFO
	edma_set_dest (dma_ch1, (unsigned long)(dmaphysdest1), INCR, W32BIT);
	edma_set_src_index (dma_ch1, srcbidx, srccidx);
	edma_set_dest_index (dma_ch1, desbidx, descidx);
	edma_set_transfer_params (dma_ch1, acnt, bcnt, ccnt, BRCnt, ABSYNC);

	/* Enable the Interrupts on Channel 1 */
	edma_read_slot (dma_ch1, &param_set);
	//param_set.opt &= ~(1 << ITCINTEN_SHIFT | 1 << STATIC_SHIFT | 1 << TCCHEN_SHIFT | 1 << ITCCHEN_SHIFT );  
	//// Changed so it only triggers after ping or pong is full TM
	param_set.opt |= (1 << TCINTEN_SHIFT); // | 1 << TCCHEN_SHIFT); did not like this
	param_set.opt |= EDMA_TCC(EDMA_CHAN_SLOT(dma_ch1));
	edma_write_slot(dma_ch1, &param_set);
	DMA_PRINTK("\n opt for ch %u", param_set.opt); 


	//Test to see if we can get CCNT to remain the same
	edma_set_src (dma_slot1, spi_fifo, FIFO, W32BIT); // need to put in addres of FIFO
	edma_set_dest (dma_slot1, (unsigned long)(dmaphysdest1), INCR, W32BIT);
	edma_set_src_index (dma_slot1, srcbidx, srccidx);
	edma_set_dest_index (dma_slot1, desbidx, descidx);
	edma_set_transfer_params (dma_slot1, acnt, bcnt, ccnt, BRCnt, ABSYNC);

	edma_write_slot(dma_slot1, &param_set);
	



	edma_set_src (dma_slot2, spi_fifo, FIFO, W32BIT); // change to same src
	edma_set_dest (dma_slot2, (unsigned long)(dmaphysdest2), INCR, W32BIT);
	edma_set_src_index (dma_slot2, srcbidx, srccidx);
	edma_set_dest_index (dma_slot2, desbidx, descidx);
	edma_set_transfer_params (dma_slot2, acnt, bcnt, ccnt, BRCnt, ABSYNC);

	/* Enable the Interrupts on Channel 2 */
	edma_read_slot (dma_slot2, &param_set);
	//param_set.opt &= ~(1 << ITCINTEN_SHIFT | 1 << STATIC_SHIFT | 1 << TCINTEN_SHIFT | 1 << ITCCHEN_SHIFT);  //Same here TM
	param_set.opt |= (1 << TCINTEN_SHIFT); // | 1 << TCCHEN_SHIFT);
	param_set.opt |= EDMA_TCC(EDMA_CHAN_SLOT(dma_ch1)); // This May be key
	edma_write_slot(dma_slot2, &param_set);
	DMA_PRINTK("\n opt for link%u slot %u",dma_slot2, param_set.opt); 



	result = edma_start(dma_ch1);
	if (result != 0) {
		DMA_PRINTK ("edma3_fifotomemcpytest_dma_link: davinci_start_dma failed \n");
	}

	return result;
}


static int stop_ping_pong(int *ch, int *slot1, int *slot2){

	edma_stop(*ch);
	edma_free_channel(*ch);
	edma_free_slot(*slot1);
	edma_free_slot(*slot2);
	DMA_PRINTK ("Stoped channels and freed them");
	return 0;
}



/* File Operations to Communicate with Userland */
struct file_operations mighty_fops = {
	owner:	 	THIS_MODULE,
	read:	 	ebic_read,
	write:	 	ebic_write,
	unlocked_ioctl:	ebic_ioctl,
	open:	 	ebic_open,
	release: 	ebic_release,
};

module_init(edma_test_init);
module_exit(edma_test_exit);

MODULE_AUTHOR("Ephemeron Labs, Inc.");
MODULE_DESCRIPTION("EDMA3 Driver for the Mighty EBIC Platform");
MODULE_LICENSE("GPL");
