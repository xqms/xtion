#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

__visible struct module __this_module
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
	{ 0x695c5f49, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0x31b4e139, __VMLINUX_SYMBOL_STR(vb2_ioctl_reqbufs) },
	{ 0x689bcaf, __VMLINUX_SYMBOL_STR(kmalloc_caches) },
	{ 0xf9a482f9, __VMLINUX_SYMBOL_STR(msleep) },
	{ 0xbdb60fcd, __VMLINUX_SYMBOL_STR(dev_set_drvdata) },
	{ 0xa8c9fcf1, __VMLINUX_SYMBOL_STR(v4l2_device_unregister) },
	{ 0x9bfb808, __VMLINUX_SYMBOL_STR(vb2_ioctl_streamon) },
	{ 0xca1173b6, __VMLINUX_SYMBOL_STR(usb_kill_urb) },
	{ 0x3975358b, __VMLINUX_SYMBOL_STR(vb2_ops_wait_prepare) },
	{ 0x768ae0c4, __VMLINUX_SYMBOL_STR(__video_register_device) },
	{ 0xda30f97c, __VMLINUX_SYMBOL_STR(v4l2_device_register) },
	{ 0xdbf9cc95, __VMLINUX_SYMBOL_STR(vb2_vmalloc_memops) },
	{ 0x469b798d, __VMLINUX_SYMBOL_STR(dev_err) },
	{ 0x8f64aa4, __VMLINUX_SYMBOL_STR(_raw_spin_unlock_irqrestore) },
	{ 0x6f0583ad, __VMLINUX_SYMBOL_STR(vb2_fop_mmap) },
	{ 0xe3fe4067, __VMLINUX_SYMBOL_STR(vb2_ioctl_qbuf) },
	{ 0x4bc494c6, __VMLINUX_SYMBOL_STR(usb_deregister) },
	{ 0xab442f5e, __VMLINUX_SYMBOL_STR(video_unregister_device) },
	{ 0x99fef161, __VMLINUX_SYMBOL_STR(vb2_buffer_done) },
	{ 0x9166fada, __VMLINUX_SYMBOL_STR(strncpy) },
	{ 0x4472c113, __VMLINUX_SYMBOL_STR(usb_control_msg) },
	{ 0x20d5df5a, __VMLINUX_SYMBOL_STR(v4l2_fh_release) },
	{ 0xc40170ed, __VMLINUX_SYMBOL_STR(vb2_ioctl_dqbuf) },
	{ 0x12153db5, __VMLINUX_SYMBOL_STR(_dev_info) },
	{ 0x1e0ec7bf, __VMLINUX_SYMBOL_STR(usb_submit_urb) },
	{ 0xf755824a, __VMLINUX_SYMBOL_STR(usb_get_dev) },
	{ 0x6dfe63cb, __VMLINUX_SYMBOL_STR(video_devdata) },
	{ 0xf0fdf6cb, __VMLINUX_SYMBOL_STR(__stack_chk_fail) },
	{ 0x9d29304b, __VMLINUX_SYMBOL_STR(usb_put_dev) },
	{ 0xa202a8e5, __VMLINUX_SYMBOL_STR(kmalloc_order_trace) },
	{ 0xbdfb6dbb, __VMLINUX_SYMBOL_STR(__fentry__) },
	{ 0xad99eb6b, __VMLINUX_SYMBOL_STR(kmem_cache_alloc_trace) },
	{ 0x9327f5ce, __VMLINUX_SYMBOL_STR(_raw_spin_lock_irqsave) },
	{ 0x4d7fefa1, __VMLINUX_SYMBOL_STR(v4l2_fh_open) },
	{ 0xe623f854, __VMLINUX_SYMBOL_STR(vb2_ioctl_querybuf) },
	{ 0x37a0cba, __VMLINUX_SYMBOL_STR(kfree) },
	{ 0x69acdf38, __VMLINUX_SYMBOL_STR(memcpy) },
	{ 0xca026080, __VMLINUX_SYMBOL_STR(usb_register_driver) },
	{ 0x7df4495b, __VMLINUX_SYMBOL_STR(vb2_ops_wait_finish) },
	{ 0x28318305, __VMLINUX_SYMBOL_STR(snprintf) },
	{ 0xd56c7b61, __VMLINUX_SYMBOL_STR(vb2_ioctl_streamoff) },
	{ 0xa2218243, __VMLINUX_SYMBOL_STR(vb2_queue_release) },
	{ 0xa996bdf6, __VMLINUX_SYMBOL_STR(dev_get_drvdata) },
	{ 0x32630f24, __VMLINUX_SYMBOL_STR(usb_free_urb) },
	{ 0xb56b1886, __VMLINUX_SYMBOL_STR(video_ioctl2) },
	{ 0x19272ed2, __VMLINUX_SYMBOL_STR(usb_alloc_urb) },
	{ 0x95f25634, __VMLINUX_SYMBOL_STR(vb2_queue_init) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=videobuf2-core,videodev,videobuf2-vmalloc";

MODULE_ALIAS("usb:v1D27p0601d*dc*dsc*dp*ic*isc*ip*in*");

MODULE_INFO(srcversion, "78B80CB99F454C32D433841");
