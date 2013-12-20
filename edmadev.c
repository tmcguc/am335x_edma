/* 
 * Simple userspace interface to EDMA
 * 
 * Andrew Righter (andrew.righter@gmail.com)
 * Ephemeron-Labs, LLC.
 *
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include "include/hw_types.h"			// required for HWREG macro

static int __init edmadev_init(void) {

	unsigned int tmpPID;
	
	printk("Initializing EDMA Kernel Driver\n");
	tmpPID = HWREG(0xb6f59000);
	printk("EDMA3CC PID = %i", tmpPID); 

	return 0;
}


static void __exit edmadev_exit(void) {

	printk("EDMA Dev Exit.\n");
}

module_init(edmadev_init);
module_exit(edmadev_exit);

MODULE_AUTHOR("Andrew Righter <andrew.righter@gmail.com");
MODULE_DESCRIPTION("EDMA userspace interface");
MODULE_LICENSE("GPL v2");

