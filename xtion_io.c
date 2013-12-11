/*
 * USB I/O for ASUS xtion
 *
 * Author: Max Schwarz <max.schwarz@online.de>
 */

#include "xtion_io.h"
#include "xtion.h"

#include <linux/usb.h>
#include <linux/slab.h>

int xtion_control(struct xtion* xtion, __u8 *src, __u16 size, __u8 *dst, __u16 *dst_size)
{
	int ret = 0;
	int tries;
	struct XtionHeader *header = (struct XtionHeader*)src;
	struct XtionReplyHeader *reply = (struct XtionReplyHeader*)dst;

	header->id = xtion->message_id++;

	ret = usb_control_msg(
		xtion->dev,
		usb_sndctrlpipe(xtion->dev, 0),
		0,
		USB_TYPE_VENDOR | USB_DIR_OUT | USB_RECIP_DEVICE,
		0,
		0,
		(__u8*)src,
		size,
		500
	);

	if(ret < 0) {
		dev_err(&xtion->dev->dev, "Could not send USB control request: %d", ret);
		return ret;
	}

	if(!dst)
		return 0;

	for(tries = 0; tries < 10; ++tries) {
		ret = usb_control_msg(
			xtion->dev,
			usb_sndctrlpipe(xtion->dev, 0),
			0,
			USB_TYPE_VENDOR | USB_DIR_IN | USB_RECIP_DEVICE,
			0,
			0,
			dst,
			*dst_size,
			500
		);

		if(ret < 0) {
			dev_err(&xtion->dev->dev, "Could not read USB control response: %d", ret);
			return ret;
		}

		if(ret >= sizeof(struct XtionReplyHeader)
		        && reply->header.magic == XTION_MAGIC_DEV
		        && reply->header.id == header->id) {
			*dst_size = ret;
			return 0;
		}

		msleep(10);
	}

	return -ETIMEDOUT;
}

int xtion_read_version(struct xtion* xtion)
{
	__u8 response_buffer[sizeof(struct XtionReplyHeader) + sizeof(struct XtionVersion)];
	__u16 response_size = sizeof(response_buffer);
	struct XtionHeader header;
	int ret;
	struct XtionReplyHeader* reply = (struct XtionReplyHeader*)response_buffer;

	header.magic = XTION_MAGIC_HOST;
	header.size = 0;
	header.opcode = XTION_OPCODE_GET_VERSION;
	header.id = 0;

	ret = xtion_control(xtion, (__u8*)&header, sizeof(header), response_buffer, &response_size);

	if(ret < 0) {
		dev_err(&xtion->dev->dev, "Could not read version: %d\n", ret);
		return ret;
	}

	if(response_size != sizeof(struct XtionReplyHeader) + sizeof(struct XtionVersion) || reply->header.magic != XTION_MAGIC_DEV) {
		dev_err(&xtion->dev->dev, "Invalid response\n");
		return -EIO;
	}

	memcpy(&xtion->version, response_buffer + sizeof(struct XtionReplyHeader), sizeof(struct XtionVersion));

	return 0;
}

int xtion_read_fixed_params(struct xtion* xtion)
{
	__u8 response_buffer[sizeof(struct XtionReplyHeader) + sizeof(struct XtionFixedParams)];
	__u16 response_size = sizeof(response_buffer);
	struct XtionFixedParamRequest request;
	__u32 data_read = 0;
	int ret;
	struct XtionReplyHeader* reply = (struct XtionReplyHeader*)response_buffer;
	__u8 *dest = (__u8*)&xtion->fixed;

	request.header.magic = XTION_MAGIC_HOST;
	request.header.size = 0;
	request.header.opcode = XTION_OPCODE_GET_FIXED_PARAMS;
	request.header.id = 0;

	while(data_read < sizeof(struct XtionFixedParams)) {
		__u32 data_size;

		request.addr = __cpu_to_le16(data_read / 4);

		ret = xtion_control(xtion, (__u8*)&request, sizeof(request), response_buffer, &response_size);

		if(ret != 0) {
			dev_err(&xtion->dev->dev, "Could not read fixed params at addr %d: %d", data_read/4, ret);
			return ret;
		}

		if(response_size < sizeof(struct XtionReplyHeader) || reply->header.magic != XTION_MAGIC_DEV) {
			dev_err(&xtion->dev->dev, "Invalid response");
			return -EIO;
		}

		data_size = response_size - sizeof(struct XtionReplyHeader);

		if(data_size == 0)
			break;

		memcpy(dest + data_read, response_buffer + sizeof(struct XtionReplyHeader), data_size);
		data_read += data_size;
	}

	return 0;
}

int xtion_read_serial_number(struct xtion *xtion)
{
	__u8 response_buffer[sizeof(struct XtionReplyHeader) + SERIAL_NUMBER_MAX_LEN];
	__u16 response_size = sizeof(response_buffer);
	struct XtionHeader header;
	int ret;
	struct XtionReplyHeader* reply = (struct XtionReplyHeader*)response_buffer;
	int size;

	header.magic = XTION_MAGIC_HOST;
	header.size = 0;
	header.opcode = XTION_OPCODE_GET_SERIAL_NUMBER;
	header.id = 1;

	ret = xtion_control(xtion, (__u8*)&header, sizeof(header), response_buffer, &response_size);

	if(ret < 0) {
		dev_err(&xtion->dev->dev, "Could not read serial number: %d\n", ret);
		return ret;
	}

	if(response_size < sizeof(struct XtionReplyHeader) || reply->header.magic != XTION_MAGIC_DEV) {
		dev_err(&xtion->dev->dev, "Invalid response (size %d, magic 0x%X)\n", response_size, reply->header.magic);
		return -EIO;
	}

	size = response_size - sizeof(struct XtionReplyHeader);
	memcpy(xtion->serial_number, response_buffer + sizeof(struct XtionReplyHeader), size);
	xtion->serial_number[size] = 0;

	return 0;
}

int xtion_set_param(struct xtion *xtion, __u16 parameter, __u16 value)
{
	struct XtionSetParamRequest packet;
	__u8 response_buffer[sizeof(struct XtionReplyHeader) + 20];
	__u16 response_size = sizeof(response_buffer);
	struct XtionReplyHeader *reply = (struct XtionReplyHeader*)response_buffer;
	int rc;

	packet.header.magic = XTION_MAGIC_HOST;
	packet.header.opcode = XTION_OPCODE_SET_PARAM;
	packet.header.size = 4;

	packet.param = parameter;
	packet.value = value;

	rc = xtion_control(xtion, (__u8*)&packet, sizeof(packet), response_buffer, &response_size);
	if(rc < 0) {
		dev_err(&xtion->dev->dev, "Could not set parameter: %d\n", rc);
		return rc;
	}

	if(response_size < sizeof(struct XtionReplyHeader) || reply->header.magic != XTION_MAGIC_DEV) {
		dev_err(&xtion->dev->dev, "Invalid response (size %d, magic 0x%X)\n", response_size, reply->header.magic);
		return -EIO;
	}

	if(reply->error != 0) {
		dev_err(&xtion->dev->dev, "param error: %d\n", reply->error);
	}

	dev_info(&xtion->dev->dev, "changed param %d to %d\n", parameter, value);

	return 0;
}
