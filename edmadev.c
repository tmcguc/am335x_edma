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

#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>
#include <linux/omap-dma.h>


static int __init edmadev_init(void) {

	printk("EDMA Dev Init.\n");
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

