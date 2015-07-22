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
	{ 0x9e2c9c0b, "struct_module" },
	{ 0x16d2ab7f, "simple_statfs" },
	{ 0x48eb4d4e, "generic_delete_inode" },
	{ 0xd92ef7a4, "kill_litter_super" },
	{ 0x9edbecae, "snprintf" },
	{ 0x735a0bd5, "native_io_delay" },
	{ 0x9ccb2622, "finish_wait" },
	{ 0x1000e51, "schedule" },
	{ 0x33d92f9a, "prepare_to_wait" },
	{ 0xc8b57c27, "autoremove_wake_function" },
	{ 0x37a0cba, "kfree" },
	{ 0x5a34a45c, "__kmalloc" },
	{ 0x945bc6a7, "copy_from_user" },
	{ 0x88053514, "pci_set_consistent_dma_mask" },
	{ 0x2cf190e3, "request_irq" },
	{ 0xfbed20d8, "ioremap_nocache" },
	{ 0xb24475e0, "pci_bus_write_config_dword" },
	{ 0xf8c2de84, "pci_bus_read_config_dword" },
	{ 0x174afa96, "pci_bus_read_config_word" },
	{ 0xd6db7d36, "pci_bus_read_config_byte" },
	{ 0xb3a78ffa, "pci_set_master" },
	{ 0x6c324f5a, "pci_enable_device" },
	{ 0xbe499d81, "copy_to_user" },
	{ 0x642e54ac, "__wake_up" },
	{ 0x435b566d, "_spin_unlock" },
	{ 0x973873ab, "_spin_lock" },
	{ 0xe52947e7, "__phys_addr" },
	{ 0x770308d, "dma_alloc_coherent" },
	{ 0x3c2c5af5, "sprintf" },
	{ 0xaa74e2c9, "iput" },
	{ 0x58081dc0, "d_alloc_root" },
	{ 0x6736d55f, "simple_dir_operations" },
	{ 0x98ea9a9b, "simple_dir_inode_operations" },
	{ 0xa58b22ed, "d_rehash" },
	{ 0xafd2e336, "d_instantiate" },
	{ 0x6121328b, "dput" },
	{ 0xab9bf0, "d_alloc" },
	{ 0x74cc238d, "current_kernel_time" },
	{ 0x480b7b2d, "new_inode" },
	{ 0x8e8f1ab0, "get_sb_single" },
	{ 0x3926b9e9, "register_filesystem" },
	{ 0x30238b06, "__pci_register_driver" },
	{ 0xde0bdcff, "memset" },
	{ 0x56264465, "pci_disable_device" },
	{ 0xf20dabd8, "free_irq" },
	{ 0xea147363, "printk" },
	{ 0xf4bb0d5e, "dma_free_coherent" },
	{ 0x79316443, "pci_unregister_driver" },
	{ 0xb283ccea, "unregister_filesystem" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";

MODULE_ALIAS("pci:v000010EEd00000300sv*sd*bc*sc*i*");

MODULE_INFO(srcversion, "38EBFB4165F3DFADD9819F7");
