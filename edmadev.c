/* 
 * Simple userspace interface to EDMA
 * 
 * Andrew Righter (andrew.righter@gmail.com)
 * Ephemeron-Labs, LLC.
 *
*/

#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>
#include <linux/omap-dma.h>

static void __exit edma_exit(void)
{
	platform_device_unregister(pdev0);
	if (pdev1)
		platform_device_unregister(pdev1);
	platform_driver_unregister(&edma_driver);
}
module_exit(edma_exit);

MODULE_AUTHOR("Andrew Righter <andrew.righter@gmail.com");
MODULE_DESCRIPTION("EDMA userspace interface");
MODULE_LICENSE("GPL v2");

