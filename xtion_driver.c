/*
 * ASUS xtion kernel driver
 *
 * Author: Max Schwarz <max.schwarz@online.de>
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-vmalloc.h>

#include "xtion.h"
#include "xtion_io.h"
#include "xtion_endpoint.h"

static struct usb_device_id id_table[] = {
	{ USB_DEVICE(VENDOR_ID, PRODUCT_ID) },
	{}
};
MODULE_DEVICE_TABLE(usb, id_table);

static void color_start(struct xtion_endpoint* endp)
{
}

static void color_data(struct xtion_endpoint* endp, const __u8* data, unsigned int size)
{
}

static void color_end(struct xtion_endpoint* endp)
{
	dev_info(&endp->xtion->dev->dev, "got complete image frame of size %d\n", endp->packet_data_size);
}

static const struct xtion_endpoint_config color_config = {
	.addr         = 0x81,
	.start_id     = 0x8100,
	.end_id       = 0x8500,
	.handle_start = color_start,
	.handle_data  = color_data,
	.handle_end   = color_end
};

static int xtion_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
	struct usb_device *udev = interface_to_usbdev(interface);
	struct xtion *xtion = NULL;
	int ret = -ENOMEM;

	xtion = kzalloc(sizeof(struct xtion), GFP_KERNEL);
	if(!xtion) {
		dev_err(&interface->dev, "Out of memory\n");
		goto error;
	}

	xtion->dev = usb_get_dev(udev);

	usb_set_intfdata(interface, xtion);

	ret = xtion_read_version(xtion);
	if(ret != 0)
		goto error_release;

	dev_info(&xtion->dev->dev, "Found ASUS Xtion with firmware version %d.%d.%d, chip: 0x%X, fpga: %d\n",
		xtion->version.major, xtion->version.minor, xtion->version.build,
		xtion->version.chip,
		xtion->version.fpga
	);

	if(xtion->version.major * 256 + xtion->version.minor < 7 * 256 + 5) {
		dev_err(&xtion->dev->dev, "The firmware is too old. Please update it following the instructions at the ASUS support site.\n");
		ret = -EIO;
		goto error_release;
	}

	ret = xtion_read_fixed_params(xtion);
	if(ret != 0)
		goto error_release;

	ret = xtion_read_serial_number(xtion);
	if(ret != 0)
		goto error_release;

	dev_info(&xtion->dev->dev, "ID: %s\n", xtion->serial_number);

	/* Setup video device */
	ret = v4l2_device_register(&interface->dev, &xtion->v4l2_dev);
	if(ret != 0)
		goto error_release;

	/* Setup endpoints */
	ret = xtion_endpoint_init(&xtion->color, xtion, &color_config);
	if(ret != 0)
		goto error_unregister;

	return 0;

error_unregister:
	v4l2_device_unregister(&xtion->v4l2_dev);
error_release:
	usb_set_intfdata(interface, NULL);
	usb_put_dev(xtion->dev);
error:
	kfree(xtion);
	return ret;
}

static void xtion_disconnect(struct usb_interface *interface)
{
	struct xtion* xtion = usb_get_intfdata(interface);
	usb_set_intfdata(interface, NULL);

	xtion_endpoint_release(&xtion->color);

	v4l2_device_unregister(&xtion->v4l2_dev);

	usb_put_dev(xtion->dev);

	kfree(xtion);

	dev_info(&interface->dev, "xtion disconnected\n");
}


static struct usb_driver xtion_driver = {
	.name       = "xtion",
	.probe      = xtion_probe,
	.disconnect = xtion_disconnect,
	.id_table   = id_table
};

static int __init xtion_init(void)
{
	int ret = 0;

	ret = usb_register(&xtion_driver);

	return ret;
}

static void __exit xtion_exit(void)
{
	usb_deregister(&xtion_driver);
}

module_init(xtion_init);
module_exit(xtion_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
