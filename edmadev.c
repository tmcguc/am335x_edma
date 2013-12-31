/*****************************************************************************/

/* 
  EDMADEV Kernel Driver for Beaglebone Black
  
  Currently designed to work on ARM
  	AM335x Cortex-A8 MPU
 	TESTED: Beaglebone Black Rev: A5
 
  Andrew Righter (andrew.righter@gmail.com)
  Terrence McGuckin (terrence@ephemeron-labs.com)
  for Ephemeron-Labs, LLC.


  CAUTION: We commented the shit out of this code.
 
*/

/*****************************************************************************/

#include <linux/fs.h>		/* For file_operations struct */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/edma.h>
#include <linux/err.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_dma.h>
#include <linux/of_irq.h>
#include <linux/pm_runtime.h>

#include <linux/platform_data/edma.h>

/*****************************************************************************/

/* 
	Base Address Memory Mapping Offsets 
	(REF: TRM Section 2.1) 
*/
#define EDMA3CC_BASE		0x49000000 		/*  to 0x490F_FFFF */
#define EDMATC0_BASE		0x49800000 		/*  to 0x498F_FFFF */
#define EDMATC1_BASE		0x49900000 		/*  to 0x499F_FFFF */
#define EDMATC2_BASE		0x49A00000 		/*  to 0x49AF_FFFF */

/*
	Channel Mapping: EDMADEV Configuration

	Offset based off of EDMA3CC_BASE
*/
#define EDMA_DCHMAP			0x0100  /* 0x100 - 0x1FC (DCHMAP0 - DCHMAP63) */
#define	EDMA_DCHMAP_EVNT17	0x144	/* Offset for EVENT 17 (4 byte increments) (0x100 + 0x44 = 0x144) */
#define EDMA_DCHMAP_EVNT20	0x150	/* Offset for EVENT 20 (4 byte increments) (0x100 + 0x50 = 0x150) */

/*
	Event Enable Register: EDMADEV Configuration

	Offset based off of EDMA3CC_BASE
*/
#define EDMA_ER 			0x1000 	/* READ-ONLY REGISTER (REF: Page 1409 of TRM) */
#define EDMA_EER 			0x1020	/* READ-ONLY REGISTER - To write to EER you have to set the bit in EESR */
#define EDMA_EESR 			0x1030  /* Set bit 17 |1 << 17| (writing 0 has no effect) */
#define EDMA_EECR 			0x1028  /* Clear bit 17 |1 << 17| (writing 0 has no effect, lets you change things without messing up other events) */ 

/*
	PaRAM Set Number: EDMADEV Configuration

	Offset based off of EDMA3CC_BASE
*/
#define EDMA_PARM		0x4000	/* 128 param entries */
#define EDMA_PARM_17	0x4220	/* EDMA3CC_BASE + EDMA_PARM_17 = Offset for Entry 17 */
#define EDMA_PARM_20	0x4280 	/* EDMA3CC_BASE + EDMA_PARM_20 = Offset for Entry 20 */ 
#define EDMA_PARM_64	0x4800 	/* EDMA3CC_BASE + EDMA_PARM_64 = Offset for Entry 64 */
#define PARM_OFFSET(param_no)	(EDMA_PARM + ((param_no) << 5))								/* How does this work? */

/* 
	PARM_OPT: EDMADEV CONFIGURATION
	(REF: TRM Section 11.3.3 Page 1247)

	Event 17 Configuration (SPI Trigger)
	Event 20 Configuration (Manual Trigger)
*/
#define PARM_OPT			0x00 	/* Channel Options: Transfer configuration options */
#define PARM_OPT_PRIV				// 17/20: 	PRIV |0 << 31| 
#define PARM_OPT_TCIENTEN   		// 17/20: 	TCIENTEN |1 << 20|
#define PARM_OPT_TCC				// 17/20: 	TCC |TCC interrupt output << 12| 
#define PARM_OPT_FWID 				// 17/20: 	FWID |0x2 << 8|, NOT NEEDED 
#define PARM_OPT_STATIC				// 17/20:	STATIC |0 << 3| , STATIC |1 << 3|
#define PARM_OPT_SYNCDIM 			// 17/20: 	SYNCDIM |1 << 2|
#define PARM_OPT_DAM				// 17/20: 	DAM |0 << 1| 
#define PARM_OPT_SAM				// 17/20: 	SAM |1 << 0| , SAM |0 << 0|

/* 
	Offsets matching "struct edmacc_param" 

	Each PaRAM Set is 32 bytes wide, these are offsets for that set
	Each Offset (EX: PARM_SRC is a 32 bit word)
*/
#define PARM_SRC			0x04 	/* Event 17 - SRC = 0x480301a0 ( McSPI0 CS0 DAFRX) */
									/* Event 20 - I define this */
#define PARM_A_B_CNT		0x08 	/* ACNT = 4 (4 bytes in 32 bit word use this at first, test other values latter) */
									/* BCNT = 1 (Variable 1 to 8 for the number of ADC channels) */
#define PARM_DST			0x0c 	/* DST = set to PING or PONG BLOCK *you choose these starting block for PING and PONG */
#define PARM_SRC_DST_BIDX	0x10 	/* SRCBIDX |0x0| (source BCNT index set to 0 constant addressing mode) */
									/* DSTBIDX |0x4| (I think this is correct for every BCNt we increment 4 bytes size in offsets in a frame) */
#define PARM_LINK_BCNTRLD	0x14 	/* LINK address *you set this PING or PONG PaRAM address */
									/* BCNTRLD |1| ( only revelvant in A sychronzied transfer set to BCNT */
#define PARM_SRC_DST_CIDX	0x18 	/* SRCCIDX should be 0 ( source is in constant address mode) */
									/* DSTCIDX ( should be ACNT x BCNT) for AB sync transfers */
#define PARM_CCNT			0x1c 	/* CCNT ( variable should be (samples_per_point) x (points_per_line) */
									/* RSVD = 0x0000 (always write 0 to here) Write as 32 bit word */


/*
	Offsets for Interupt Registers: EDMADEV Configuration
	
	Offsets based off of EDMA3CC_BASE 

	NOTE: Either the global or shadow regions for interrupts should be enabled but not both!!
		  Transfer completion codes can be mapped to different IPR bits, we need this to have two interrupts for ping and pong buffers
*/
#define	EDMA_IER	0x1050  		/* All we need for our purposes is to make sure bit 17 is set */
#define EDMA_IESR 	0x1060 			/*  IESR |1 << 17| (write a one into it) */
#define EDMA_IECR  	0x1058 			/*  IECR |1 << 17| */
#define EDMA_IEPR	0x1068 			/* if value in corresponding bit is 1 than the transfer has occurred for corresponding channel, must be cleared from EDMA_IECR */
#define EDMA_IECR 	0x1070 			/* Write here to clear interrupt after CPU grabs data from ping or pong buffer */

/*****************************************************************************/

#define EDMADEV_MAJOR 			234		/* DEBUG: MAJOR REVISION, DEVICE NODE */

/*****************************************************************************/

static int debug_enable = 0;
module_param(debug_enable, int, 0);
MODULE_PARM_DESC(debug_enable, "enable module debug mode.");

/*****************************************************************************/

/* 
	Function Declarations

	Note: I will remove unneeded functions

*/

/*****************************************************************************/

static void __iomem *edmacc_regs_base[EDMA_MAX_CC];

static inline unsigned int edma_read(unsigned ctlr, int offset)
{
	return (unsigned int)__raw_readl(edmacc_regs_base[ctlr] + offset);
}

static inline void edma_write(unsigned ctlr, int offset, int val)
{
	__raw_writel(val, edmacc_regs_base[ctlr] + offset);
}
static inline void edma_modify(unsigned ctlr, int offset, unsigned and,
		unsigned or)
{
	unsigned val = edma_read(ctlr, offset);
	val &= and;
	val |= or;
	edma_write(ctlr, offset, val);
}
static inline void edma_and(unsigned ctlr, int offset, unsigned and)
{
	unsigned val = edma_read(ctlr, offset);
	val &= and;
	edma_write(ctlr, offset, val);
}
static inline void edma_or(unsigned ctlr, int offset, unsigned or)
{
	unsigned val = edma_read(ctlr, offset);
	val |= or;
	edma_write(ctlr, offset, val);
}
static inline unsigned int edma_read_array(unsigned ctlr, int offset, int i)
{
	return edma_read(ctlr, offset + (i << 2));
}
static inline void edma_write_array(unsigned ctlr, int offset, int i,
		unsigned val)
{
	edma_write(ctlr, offset + (i << 2), val);
}
static inline void edma_modify_array(unsigned ctlr, int offset, int i,
		unsigned and, unsigned or)
{
	edma_modify(ctlr, offset + (i << 2), and, or);
}
static inline void edma_or_array(unsigned ctlr, int offset, int i, unsigned or)
{
	edma_or(ctlr, offset + (i << 2), or);
}
static inline void edma_or_array2(unsigned ctlr, int offset, int i, int j,
		unsigned or)
{
	edma_or(ctlr, offset + ((i*2 + j) << 2), or);
}
static inline void edma_write_array2(unsigned ctlr, int offset, int i, int j,
		unsigned val)
{
	edma_write(ctlr, offset + ((i*2 + j) << 2), val);
}
static inline unsigned int edma_shadow0_read(unsigned ctlr, int offset)
{
	return edma_read(ctlr, EDMA_SHADOW0 + offset);
}
static inline unsigned int edma_shadow0_read_array(unsigned ctlr, int offset,
		int i)
{
	return edma_read(ctlr, EDMA_SHADOW0 + offset + (i << 2));
}
static inline void edma_shadow0_write(unsigned ctlr, int offset, unsigned val)
{
	edma_write(ctlr, EDMA_SHADOW0 + offset, val);
}
static inline void edma_shadow0_write_array(unsigned ctlr, int offset, int i,
		unsigned val)
{
	edma_write(ctlr, EDMA_SHADOW0 + offset + (i << 2), val);
}
static inline unsigned int edma_parm_read(unsigned ctlr, int offset,
		int param_no)
{
	return edma_read(ctlr, EDMA_PARM + offset + (param_no << 5));
}
static inline void edma_parm_write(unsigned ctlr, int offset, int param_no,
		unsigned val)
{
	edma_write(ctlr, EDMA_PARM + offset + (param_no << 5), val);
}
static inline void edma_parm_modify(unsigned ctlr, int offset, int param_no,
		unsigned and, unsigned or)
{
	edma_modify(ctlr, EDMA_PARM + offset + (param_no << 5), and, or);
}
static inline void edma_parm_and(unsigned ctlr, int offset, int param_no,
		unsigned and)
{
	edma_and(ctlr, EDMA_PARM + offset + (param_no << 5), and);
}
static inline void edma_parm_or(unsigned ctlr, int offset, int param_no,
		unsigned or)
{
	edma_or(ctlr, EDMA_PARM + offset + (param_no << 5), or);
}

static inline void set_bits(int offset, int len, unsigned long *p)
{
	for (; len > 0; len--)
		set_bit(offset + (len - 1), p);
}

static inline void clear_bits(int offset, int len, unsigned long *p)
{
	for (; len > 0; len--)
		clear_bit(offset + (len - 1), p);
}

/*****************************************************************************/

/*
		Kernel Driver INIT and EXIT code
*/

/*****************************************************************************/

static int __init edmadev_init(void) {

	int ret;
	printk("EDMADEV Example Init - debug mode is %s\n",
			debug_enable ? "enabled" : "disabled");

	ret = register_chrdev(EDMADEV_MAJOR, "hello1", &hello_fops);
		if (ret < 0) {
			printk("Error registering hello device\n");
			goto hello_fail1;
		}

	printk("Hello: registered module successfully");

	/* Init Processing Here */
	return 0;

hello_fail1:
	return ret;
}

static void __exit edmadev_exit(void) {
	printk("*** Exiting EDMADEV Interface ***\n");
}


module_init(edmadev_init);
module_exit(edmadev_exit);

MODULE_AUTHOR("Andrew Righter <andrew.righter@gmail.com");
MODULE_DESCRIPTION("MightyEBIC EDMADEV Kernel Driver");
MODULE_LICENSE("GPL v2");

