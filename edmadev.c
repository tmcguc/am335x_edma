/* 
 * Simple userspace interface to EDMA
 * 
 * Currently designed to work on ARM
 * 	AM335x Cortex-A8 MPU
 *
 * Andrew Righter (andrew.righter@gmail.com)
 * for Ephemeron-Labs, LLC.
 *
*/

#include <linux/module.h>
#include <linux/fs.h>

//#include <linux/kernel.h>
//#include <linux/init.h>
//#include <linux/dma-mapping.h>
//#include <linux/dmaengine.h>
//#include <linux/omap-dma.h>

#define HELLO_MAJOR 234

static int debug_enable = 0;
module_param(debug_enable, int, 0);
MODULE_PARM_DESC(debug_enable, "enable module debug mode.");

struct file_operations hello_fops;

static int hello_open(struct inode *inode, struct file *file) {
	printk("hello_open: successful\n");
	return 0;
}

static int hello_release(struct inode *inode, struct file *file) {
	printk("hello_release: successful\n");
	return 0;
}

static ssize_t hello_read(struct file *file, char *buf, size_t count,
				loff_t *ptr) {
	printk("hello_read: returning 0 bytes\n");
	return 0;
}

static ssize_t hello_write(struct file *file, const char *buf, size_t count, 
				loff_t * ppos) {
	printk("hello_write: accepting zero bytes\n");
	return 0;
}

static int hello_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
				unsigned long arg) {
	printk("hello_ioctl: cmd=%1d, arg=%1d\n", cmd, arg);
	return 0;
}

// -----------------------------------

static int __init hello_init(void) {

	int ret;
	printk("Hello Example Init - debug mode is %s\n",
			debug_enable ? "enabled" : "disabled");

	ret = register_chrdev(HELLO_MAJOR, "hello1", &hello_fops);
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

struct file_operations hello_fops = {
	owner:		THIS_MODULE,
	read:		hello_read,
	write:		hello_write,
	ioctl:		hello_ioctl,
	open:		hello_open,
	release:	hello_release,
};

module_init(hello_init);
module_exit(hello_exit);

MODULE_AUTHOR("Andrew Righter <andrew.righter@gmail.com");
MODULE_DESCRIPTION("MightyEBIC EDMA Kernel Driver");
MODULE_LICENSE("GPL v2");

