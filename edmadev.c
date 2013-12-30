/* 
  EDMADEV Kernel Driver for Beaglebone Black
  
  Currently designed to work on ARM
  	AM335x Cortex-A8 MPU
 	TESTED: Beaglebone Black Rev: A5
 
  Andrew Righter (andrew.righter@gmail.com)
  for Ephemeron-Labs, LLC.
 
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
#define PARM_OFFSET(param_no)	(EDMA_PARM + ((param_no) << 5))			/* How does this work? */

/* 
	PARM_OPT: EDMADEV CONFIGURATION
	(REF: TRM Section 11.3.3 Page 1247)

	Event 17 Configuration (SPI Trigger)
	Event 20 Configuration (Manual Trigger)
*/
#define PARM_OPT			0x00
#define PARM_OPT_PRIV				// 17/20: 	PRIV |0 << 31| 
#define PARM_OPT_TCIENTEN   		// 17/20: 	TCIENTEN |1 << 20|
#define PARM_OPT_TCC				// 17/20: 	TCC |TCC interrupt output << 12| 
#define PARM_OPT_FWID 				// 17/20: 	FWID |0x2 << 8|, NOT NEEDED 
#define PARM_OPT_STATIC				// 17/20:	STATIC |0 << 3| , STATIC |1 << 3|
#define PARM_OPT_SYNCDIM 			// 17/20: 	SYNCDIM |1 << 2|
#define PARM_OPT_DAM				// 17/20: 	DAM |0 << 1| 
#define PARM_OPT_SAM				// 17/20: 	SAM |1 << 0| , SAM |0 << 0|





/* HIDE UNUSED SHIT IN HEADER FILE? */


/* Offsets matching "struct edmacc_param" */
#define PARM_SRC			0x04
#define PARM_A_B_CNT		0x08
#define PARM_DST			0x0c
#define PARM_SRC_DST_BIDX	0x10
#define PARM_LINK_BCNTRLD	0x14
#define PARM_SRC_DST_CIDX	0x18
#define PARM_CCNT			0x1c
#define PARM_SIZE			0x20

/* Offsets for EDMA CC global channel registers and their shadows */
#define SH_ER		0x00	/* 64 bits */
#define SH_ECR		0x08	/* 64 bits */
#define SH_ESR		0x10	/* 64 bits */
#define SH_CER		0x18	/* 64 bits */
#define SH_EER		0x20	/* 64 bits */
#define SH_EECR		0x28	/* 64 bits */
#define SH_EESR		0x30	/* 64 bits */
#define SH_SER		0x38	/* 64 bits */
#define SH_SECR		0x40	/* 64 bits */
#define SH_IER		0x50	/* 64 bits */
#define SH_IECR		0x58	/* 64 bits */
#define SH_IESR		0x60	/* 64 bits */
#define SH_IPR		0x68	/* 64 bits */
#define SH_ICR		0x70	/* 64 bits */
#define SH_IEVAL	0x78
#define SH_QER		0x80
#define SH_QEER		0x84
#define SH_QEECR	0x88
#define SH_QEESR	0x8c
#define SH_QSER		0x90
#define SH_QSECR	0x94
#define SH_SIZE		0x200

/* Offsets for EDMA CC global registers */
#define EDMA_REV			0x0000
#define EDMA_CCCFG			0x0004
#define EDMA_QCHMAP			0x0200	/* 8 registers */
#define EDMA_DMAQNUM		0x0240	/* 8 registers (4 on OMAP-L1xx) */
#define EDMA_QDMAQNUM		0x0260
#define EDMA_QUETCMAP		0x0280
#define EDMA_QUEPRI			0x0284
#define EDMA_EMR			0x0300	/* 64 bits */
#define EDMA_EMCR			0x0308	/* 64 bits */
#define EDMA_QEMR			0x0310
#define EDMA_QEMCR			0x0314
#define EDMA_CCERR			0x0318
#define EDMA_CCERRCLR		0x031c
#define EDMA_EEVAL			0x0320
#define EDMA_DRAE			0x0340	/* 4 x 64 bits*/
#define EDMA_QRAE			0x0380	/* 4 registers */
#define EDMA_QUEEVTENTRY	0x0400	/* 2 x 16 registers */
#define EDMA_QSTAT			0x0600	/* 2 registers */
#define EDMA_QWMTHRA		0x0620
#define EDMA_QWMTHRB		0x0624
#define EDMA_CCSTAT			0x0640

#define EDMA_M			0x1000	/* global channel registers */
#define EDMA_ECR		0x1008
#define EDMA_ECRH		0x100C
#define EDMA_SHADOW0	0x2000	/* 4 regions shadowing global channels */

#define CHMAP_EXIST	BIT(24)

#define EDMA_MAX_DMACH           64
#define EDMA_MAX_PARAMENTRY     512

#define EDMADEV_MAJOR 			234		/* DEBUG: MAJOR REVISION, DEVIE NODE */


/*****************************************************************************/

static int debug_enable = 0;
module_param(debug_enable, int, 0);
MODULE_PARM_DESC(debug_enable, "enable module debug mode.");

/*****************************************************************************/



/*****************************************************************************/

/*
		Kernel Driver INIT and EXIT code
*/


static int __init hello_init(void) {

	int ret;
	printk("Hello Example Init - debug mode is %s\n",
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

static void __exit hello_exit(void) {
	printk("*** Exiting EDMADEV Interface ***\n");
}


module_init(edmadev_init);
module_exit(edmadev_exit);

MODULE_AUTHOR("Andrew Righter <andrew.righter@gmail.com");
MODULE_DESCRIPTION("MightyEBIC EDMA Kernel Driver");
MODULE_LICENSE("GPL v2");

