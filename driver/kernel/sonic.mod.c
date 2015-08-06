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
	{ 0x53eda548, "module_layout" },
	{ 0xa4bf0727, "cdev_del" },
	{ 0xba66affe, "kmalloc_caches" },
	{ 0x5a34a45c, "__kmalloc" },
	{ 0x417e5eb2, "cdev_init" },
	{ 0x9b388444, "get_zeroed_page" },
	{ 0x4c4fef19, "kernel_stack" },
	{ 0x349cba85, "strchr" },
	{ 0xa90c928a, "param_ops_int" },
	{ 0x25ec1b28, "strlen" },
	{ 0x7a242093, "dev_set_drvdata" },
	{ 0x79aa04a2, "get_random_bytes" },
	{ 0x3c3e321c, "dma_set_mask" },
	{ 0x1b6314fd, "in_aton" },
	{ 0x780b07ac, "pci_disable_device" },
	{ 0x87a45ee9, "_raw_spin_lock_bh" },
	{ 0xa899f75f, "remove_proc_entry" },
	{ 0xeae3dfd6, "__const_udelay" },
	{ 0xd8e005d0, "pci_release_regions" },
	{ 0x85df9b6c, "strsep" },
	{ 0x7485e15e, "unregister_chrdev_region" },
	{ 0x3c2c5af5, "sprintf" },
	{ 0xe2d5255a, "strcmp" },
	{ 0x4f8b5ddb, "_copy_to_user" },
	{ 0xacc1ebd1, "param_ops_charp" },
	{ 0xeb534922, "pci_set_master" },
	{ 0xde0bdcff, "memset" },
	{ 0xe0fa1fbc, "pci_iounmap" },
	{ 0xf526695f, "current_task" },
	{ 0x27e1a049, "printk" },
	{ 0x42224298, "sscanf" },
	{ 0xafb8c6ff, "copy_user_generic_string" },
	{ 0x7ec9bfbc, "strncpy" },
	{ 0xb4390f9a, "mcount" },
	{ 0x2016dede, "set_cpus_allowed_ptr" },
	{ 0xfda85a7d, "request_threaded_irq" },
	{ 0x119af014, "cpu_bit_bitmap" },
	{ 0xef14c2d6, "cdev_add" },
	{ 0x72a98fdb, "copy_user_generic_unrolled" },
	{ 0x78764f4e, "pv_irq_ops" },
	{ 0x93fca811, "__get_free_pages" },
	{ 0x6223cafb, "_raw_spin_unlock_bh" },
	{ 0xf0fdf6cb, "__stack_chk_fail" },
	{ 0x1000e51, "schedule" },
	{ 0xd62c833f, "schedule_timeout" },
	{ 0x5c693f09, "wake_up_process" },
	{ 0x7a95413b, "pci_unregister_driver" },
	{ 0x33e4cbc6, "kmem_cache_alloc_trace" },
	{ 0xe52947e7, "__phys_addr" },
	{ 0x4302d0eb, "free_pages" },
	{ 0xddef371c, "proc_create_data" },
	{ 0x506746b6, "getrawmonotonic" },
	{ 0x37a0cba, "kfree" },
	{ 0x149ae190, "kthread_create" },
	{ 0x236c8c64, "memcpy" },
	{ 0xd7d0f222, "pci_request_regions" },
	{ 0xd1329880, "param_array_ops" },
	{ 0x12cad7a2, "pci_disable_msi" },
	{ 0x30beeca6, "dma_supported" },
	{ 0x27bdd72b, "__pci_register_driver" },
	{ 0x9edbecae, "snprintf" },
	{ 0xd1653172, "pci_enable_msi_block" },
	{ 0x9034225d, "pci_iomap" },
	{ 0x436c2179, "iowrite32" },
	{ 0x20cc8c48, "pci_enable_device" },
	{ 0x4f6b400b, "_copy_from_user" },
	{ 0xe621f01a, "dev_get_drvdata" },
	{ 0x29537c9e, "alloc_chrdev_region" },
	{ 0xf20dabd8, "free_irq" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";

MODULE_ALIAS("pci:v00001172d0000E001sv*sd*bc*sc*i*");

MODULE_INFO(srcversion, "B05B675A7459F85657DF88D");
