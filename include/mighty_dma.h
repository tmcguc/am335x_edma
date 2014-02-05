#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/dmapool.h>
#include <linux/dma-mapping.h>	
#include <linux/dmaengine.h>
#include <linux/genalloc.h>	
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/list.h>

#include <linux/platform_device.h>		/* Register/Unregister Platform Drivers */
#include <linux/platform_data/edma.h>   /* edmacc_param type */

#include "virt-dma.h"

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
#define PARM_OFFSET(param_no)	(EDMA_PARM + ((param_no) << 5))								/* EDMA_PARM + param_number + 2**5 */

/* 
	PARM_OPT: EDMADEV CONFIGURATION
	(REF: TRM Section 11.3.3 Page 1247)

	Event 17 Configuration (SPI Trigger) - 0x11205
	Event 20 Configuration (Manual Trigger)
*/
#define PARM_OPT				0x00 							/* Channel Options: Transfer configuration options */
#define PARM_OPT_PRIV			BIT(0 << 31)					// 17/20: 	PRIV |0 << 31| 
#define PARM_OPT_TCIENTEN   	BIT(1 << 20)					// 17/20: 	TCIENTEN |1 << 20|
#define PARM_OPT_TCC			(17 << 12)						// 17/20: 	TCC |TCC interrupt output << 12| 
#define PARM_OPT_FWID 			(2 << 8)						// 17/20: 	FWID |0x2 << 8|, NOT NEEDED 
#define PARM_OPT_STATIC			BIT(0 << 3)						// 17/20:	STATIC |0 << 3| , STATIC |1 << 3|
#define PARM_OPT_SYNCDIM 		BIT(1 << 2)						// 17/20: 	SYNCDIM |1 << 2|
#define PARM_OPT_DAM			BIT(0 << 1)						// 17/20: 	DAM |0 << 1| 
#define PARM_OPT_SAM			BIT(1 << 0)						// 17/20: 	SAM |1 << 0| , SAM |0 << 0|

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
//#define EDMA_IECR 	0x1070 			/* Write here to clear interrupt after CPU grabs data from ping or pong buffer */

#define CHMAP_EXIST	BIT(24)

#define EDMA_MAX_DMACH           64 	/* Needed to ensure we don't exceed MAX DMA Channels */
#define EDMA_MAX_PARAMENTRY     512 	/* Needed to ensure we don't exceed MAX PARAM Entry Slots */

#define EDMADEV_MAJOR 			234		/* DEBUG: MAJOR REVISION, DEVICE NODE */

/*****************************************************************************/

#define EDMA_CTLRS	2
#define EDMA_CHANS	32

/* Max of 16 segments per channel to conserve PaRAM slots */
#define MAX_NR_SG			16
#define EDMA_MAX_SLOTS		MAX_NR_SG
#define EDMA_DESCRIPTORS	16

#define DEBUG			1	
#define PINGCHAN		20
#define PONGCHAN		64
#define DMA_POOL_SIZE	32

/* DMA memory addresses */
struct mem_addr {
	dma_addr_t	src_addr;
	dma_addr_t	dst_addr;
};

/* For OPT configuration, see TRM 11.3.3.2 */
/* Setting SAM and FWID in the edma_set_src function */
//(PARM_OPT_PRIV | PARM_OPT_TCIENTEN | PARM_OPT_TCC | PARM_OPT_SYNCDIM),
struct edmacc_param pingset = {
	.opt = 0x11205,
	.src = 0,
	.a_b_cnt = ((8 << 16) | (4 << 0)),
	.dst = 0,
	.src_dst_bidx = 0,
	.link_bcntrld = 0,
	.src_dst_cidx = 0,
	.ccnt = 1,
};

struct edmacc_param pongset = {
	.opt = 0x11205,
	.src = 0,
	.a_b_cnt = ((8 << 16) | (4 << 0)),
	.dst = 0,
	.src_dst_bidx = 0,
	.link_bcntrld = 0,
	.src_dst_cidx = 0,
	.ccnt = 1,
};

struct edma_desc {
	//struct virt_dma_desc	vdesc;
	//struct list_head		node;
	int						absync;
	int						pset_nr;
	struct edmacc_param		pset[0];

	dma_addr_t				src_port;		 // Address: 0x480301a0 (McSPI0 CS0 DAFRX)

	struct edmacc_param		pingset;
	struct edmacc_param		pongset;

};

struct edma_cc;

/*
struct edma_chan {
	struct virt_dma_chan	vchan;
	//struct list_head		node;
	struct edma_desc		*edesc;
	struct edma_cc			*ecc;
	int						ch_num;
	bool					alloced;
	//int						slot[EDMA_MAX_SLOTS];
	//struct dma_slave_config		cfg;
	//struct dmaengine_chan_caps	caps;
};
*/

struct edma_cc {
	int						ctlr;
	struct dma_device		dma_slave;
	//struct edma_chan		slave_chans[EDMA_CHANS];
	//int					num_slave_chans;
	
	struct ebic_dmac		*dmac_a;
	struct ebic_dmac		*dmac_b;

	struct mem_addr			*kmem_addr_a;		/* Memory Address returned from dma_pool_alloc */
	struct mem_addr			*kmem_addr_b;

	//struct virt_dma_chan	vchan;
	//struct list_head		node;
	struct edma_desc		*edesc;
	struct edma_cc			*ecc;
	int						ch_num;
	bool					alloced;
	int						slot[EDMA_MAX_SLOTS];
	//struct dma_slave_config		cfg;
	//struct dmaengine_chan_caps	caps;

};

/* EBIC DMA Channel Structure */
struct ebic_dmac {
	dma_addr_t		dma_handle;		/* DMA Handle set by dma_pool_alloc */

	struct edmacc_param	*dma_test_params;

	spinlock_t		lock;
	int				dma_ch;
	int				dma_link[2];
	int				slot;
};




