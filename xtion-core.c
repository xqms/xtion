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
#include "xtion-control.h"
#include "xtion-endpoint.h"
#include "xtion-color.h"
#include "xtion-depth.h"

static struct usb_device_id id_table[] = {
	{ USB_DEVICE(VENDOR_ID, PRODUCT_ID_ASUS) },
	{ USB_DEVICE(VENDOR_ID, PRODUCT_ID_PRIMESENSE) },
	{}
};
MODULE_DEVICE_TABLE(usb, id_table);

/******************************************************************************/
/*
 * sysfs attributes
 */

ssize_t show_id(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct usb_interface *intf = container_of(dev, struct usb_interface, dev);
	struct xtion *xtion = usb_get_intfdata(intf);

	if(!xtion)
		return -ENODEV;

	snprintf(buf, PAGE_SIZE, "%s\n", xtion->serial_number);

	return strlen(buf)+1;
}

static DEVICE_ATTR(xtion_id, S_IRUGO, show_id, 0);

static int xtion_setup(void *_xtion)
{
	struct xtion *xtion = _xtion;
	struct usb_device *udev = xtion->dev;
	int ret, tries;

	msleep(3000);

	/* Switch to alternate setting 1 for isochronous transfers */
	if(xtion->flags & XTION_FLAG_ISOC) {
		ret = usb_set_interface(xtion->dev, 0, 1);
		if(ret != 0) {
			dev_err(&xtion->interface->dev, "Could not switch to isochronous alternate setting: %d", ret);
			goto error_release;
		}
	}

	/* Read firmware version and check if it is recent enough */
	ret = xtion_read_version(xtion);
	if(ret != 0) {
		usb_reset_device(udev);
		goto error_release;
	}

	dev_info(&xtion->dev->dev, "Found ASUS Xtion with firmware version %d.%d.%d, chip: 0x%X, fpga: %d\n",
		xtion->version.major, xtion->version.minor, xtion->version.build,
		xtion->version.chip,
		xtion->version.fpga
	);

	if(xtion->version.major * 256 + xtion->version.minor < 5 * 256 + 7) {
		dev_err(&xtion->dev->dev, "The firmware is too old. Please update it following the instructions at the ASUS support site.\n");
		ret = -EIO;
		goto error_release;
	}

	/* Perform a power-on reset */
	xtion_reset(xtion);

	msleep(200);

	for(tries = 0; tries < 10; ++tries) {
		/* Read fixed parameters */
		ret = xtion_read_fixed_params(xtion);
		if(ret != 0) {
			msleep(50);
			continue;
		}

		/* Read serial number and make it available through sysfs */
		ret = xtion_read_serial_number(xtion);
		if(ret != 0) {
			msleep(50);
			continue;
		}

    /* Read ConstShift parameter */
    ret = xtion_read_algorithm_params(xtion);
		if(ret != 0) {
			msleep(50);
			continue;
		}

		break;
	}

	dev_info(&xtion->dev->dev, "ID: %s\n", xtion->serial_number);
	device_create_file(&xtion->interface->dev, &dev_attr_xtion_id);

	/* Setup v4l2 device */
	ret = v4l2_device_register(&xtion->interface->dev, &xtion->v4l2_dev);
	if(ret != 0)
		goto error_release;

	/* Setup endpoints */
	ret = xtion_color_init(&xtion->color, xtion);
	if(ret != 0)
		goto error_unregister;

	ret = xtion_depth_init(&xtion->depth, xtion);
	if(ret != 0)
		goto error_release_color;

	return 0;

error_release_color:
	xtion_color_release(&xtion->color);
error_unregister:
	v4l2_device_unregister(&xtion->v4l2_dev);
error_release:
	device_remove_file(&udev->dev, &dev_attr_xtion_id);
	usb_set_intfdata(xtion->interface, NULL);

	return ret;
}

static int xtion_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
	struct usb_device *udev = interface_to_usbdev(interface);
	struct xtion *xtion = NULL;
	int ret = -ENOMEM;

	/* We only care for the data interface (0) */
	if(interface->altsetting->desc.bInterfaceNumber != 0)
		return -ENODEV;

	/* Allocate our driver structure and link it to the usb_device */
	xtion = kzalloc(sizeof(struct xtion), GFP_KERNEL);
	if(!xtion) {
		dev_err(&interface->dev, "Out of memory\n");
		goto error;
	}

	xtion->dev = usb_get_dev(udev);
	xtion->interface = interface;
	xtion->flags = 0;

	mutex_init(&xtion->control_mutex);

	usb_set_intfdata(interface, xtion);

	ret = xtion_setup(xtion);
	if (ret != 0)
		goto error;

	return 0;
error:
	usb_put_dev(xtion->dev);
	kfree(xtion);
	return ret;
}

static void xtion_disconnect(struct usb_interface *interface)
{
	struct xtion* xtion = usb_get_intfdata(interface);

	mutex_lock(&xtion->control_mutex);

	xtion_endpoint_disconnect(&xtion->color.endp);
	xtion_endpoint_disconnect(&xtion->depth.endp);

	xtion->dev = NULL;

	device_remove_file(xtion->v4l2_dev.dev, &dev_attr_xtion_id);
	v4l2_device_disconnect(&xtion->v4l2_dev);

	mutex_unlock(&xtion->control_mutex);

	dev_info(&interface->dev, "xtion disconnected\n");

	xtion_depth_release(&xtion->depth);
	xtion_color_release(&xtion->color);

	v4l2_device_unregister(&xtion->v4l2_dev);

	mutex_destroy(&xtion->control_mutex);

	usb_put_dev(xtion->dev);
	kfree(xtion);
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
