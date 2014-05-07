#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0xae4b0ceb, "module_layout" },
	{ 0x15692c87, "param_ops_int" },
	{ 0xfa2a45e, "__memzero" },
	{ 0xfbc74f64, "__copy_from_user" },
	{ 0x6f5d980d, "debug_dma_alloc_coherent" },
	{ 0xea01506a, "cdev_add" },
	{ 0x1023a56c, "cdev_init" },
	{ 0x29537c9e, "alloc_chrdev_region" },
	{ 0x83d70683, "edma_start" },
	{ 0x4e9ce9f9, "edma_link" },
	{ 0x60541702, "edma_alloc_slot" },
	{ 0x61e1850a, "edma_write_slot" },
	{ 0x85737519, "edma_read_slot" },
	{ 0xf1e0b260, "edma_set_transfer_params" },
	{ 0xcaddbd7e, "edma_set_dest_index" },
	{ 0xf7271948, "edma_set_src_index" },
	{ 0x9276ce28, "edma_set_dest" },
	{ 0x9bda4bb4, "edma_set_src" },
	{ 0xfefb6077, "edma_alloc_channel" },
	{ 0xff178f6, "__aeabi_idivmod" },
	{ 0x361bbc23, "arm_dma_ops" },
	{ 0x7485e15e, "unregister_chrdev_region" },
	{ 0x12c554f0, "cdev_del" },
	{ 0x742910f7, "debug_dma_free_coherent" },
	{ 0x7142c63c, "edma_free_slot" },
	{ 0xa31e44ba, "edma_free_channel" },
	{ 0x3635439, "edma_stop" },
	{ 0xf23fcb99, "__kfifo_in" },
	{ 0xefd6cf06, "__aeabi_unwind_cpp_pr0" },
	{ 0x4578f528, "__kfifo_to_user" },
	{ 0x2e5810c6, "__aeabi_unwind_cpp_pr1" },
	{ 0x27e1a049, "printk" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "97B4A6C2EABFA18C4805B5D");
