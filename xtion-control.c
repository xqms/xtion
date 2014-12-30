/*
 * USB control I/O for ASUS xtion
 *
 * Author: Max Schwarz <max.schwarz@online.de>
 */

#include "xtion-control.h"
#include "xtion.h"

#include <linux/usb.h>
#include <linux/slab.h>

int xtion_control(struct xtion* xtion, u8 *src, u16 size, u8 *dst, u16 *dst_size)
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
		(u8*)src,
		size,
		500
	);

	if(ret < 0) {
		dev_err(&xtion->dev->dev, "Could not send USB control request: %d", ret);
		return ret;
	}

	if(!dst)
		return 0;

//	msleep(200);

	for(tries = 0; tries < 10; ++tries) {
		ret = usb_control_msg(
			xtion->dev,
			usb_rcvctrlpipe(xtion->dev, 0),
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
	//u8 response_buffer[sizeof(struct XtionReplyHeader) + sizeof(struct XtionVersion)];
	u8 response_buffer[512];
	u16 response_size = sizeof(response_buffer);
	struct XtionHeader header;
	int ret;
	struct XtionReplyHeader* reply = (struct XtionReplyHeader*)response_buffer;

	header.magic = XTION_MAGIC_HOST;
	header.size = 0;
	header.opcode = XTION_OPCODE_GET_VERSION;
	header.id = 0;

	ret = xtion_control(xtion, (u8*)&header, sizeof(header), response_buffer, &response_size);

	if(ret < 0) {
		dev_err(&xtion->dev->dev, "Could not read version: %d\n", ret);
		return ret;
	}

	if(response_size != sizeof(struct XtionReplyHeader) + sizeof(struct XtionVersion) || reply->header.magic != XTION_MAGIC_DEV) {
		dev_err(&xtion->dev->dev, "Invalid response\n");
		return -EIO;
	}

	memcpy(&xtion->version, response_buffer + sizeof(struct XtionReplyHeader), sizeof(struct XtionVersion));

	if(xtion->version.major >= 5) {
		/*
		 * Who comes up with this kind of crap?
		 * It seems someone interpreted the build number as hex for
		 * newer xtions, so reverse that transformation
		 */
		char buf[5];
		snprintf(buf, sizeof(buf), "%x", xtion->version.build);
		WARN_ON(kstrtou16(buf, 10, &xtion->version.build));
	}

	return 0;
}

int xtion_read_algorithm_params(struct xtion* xtion)
{
	u8 response_buffer[sizeof(struct XtionReplyHeader) + sizeof(struct XtionAlgorithmParams)];
	u16 response_size = sizeof(response_buffer);
	struct XtionAlgorithmParamsRequest request;
	u32 data_read = 0;
	int ret;
	struct XtionReplyHeader* reply = (struct XtionReplyHeader*)response_buffer;
	u32 *dest = (u32*)&xtion->algorithm_params;

	request.header.magic = XTION_MAGIC_HOST;
	request.header.size = 5;
	request.header.opcode = XTION_OPCODE_GET_ALGORITHM_PARAMS;
	request.header.id = 0;

	request.param_id = HOST_PROTOCOL_ALGORITHM_DEPTH_INFO;
	request.format = HOST_PROTOCOL_ALGORITHM_FORMAT;
	request.resolution = HOST_PROTOCOL_ALGORITHM_RESOLUTION;
	request.fps = 30;

	while(data_read < sizeof(struct XtionAlgorithmParams)) {
		u32 data_size;
		request.offset = data_read / sizeof(u16);

		ret = xtion_control(xtion, (u8*)&request, sizeof(request), response_buffer, &response_size);

		if(ret != 0) {
			dev_err(&xtion->dev->dev, "Could not read algorithm params at addr %d: %d", data_read/4, ret);
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

int xtion_read_fixed_params(struct xtion* xtion)
{
	u8 response_buffer[sizeof(struct XtionReplyHeader) + sizeof(struct XtionFixedParams)];
	u16 response_size = sizeof(response_buffer);
	struct XtionFixedParamRequest request;
	u32 data_read = 0;
	int ret;
	struct XtionReplyHeader* reply = (struct XtionReplyHeader*)response_buffer;
	u8 *dest = (u8*)&xtion->fixed;

	request.header.magic = XTION_MAGIC_HOST;
	request.header.size = 1;
	request.header.opcode = XTION_OPCODE_GET_FIXED_PARAMS;
	request.header.id = 0;

	while(data_read < sizeof(struct XtionFixedParams)) {
		u32 data_size;
		request.addr = __cpu_to_le16(data_read / 4);

		ret = xtion_control(xtion, (u8*)&request, sizeof(request), response_buffer, &response_size);

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
	u8 response_buffer[sizeof(struct XtionReplyHeader) + SERIAL_NUMBER_MAX_LEN];
	u16 response_size = sizeof(response_buffer);
	struct XtionHeader header;
	int ret;
	struct XtionReplyHeader* reply = (struct XtionReplyHeader*)response_buffer;
	int size;

	header.magic = XTION_MAGIC_HOST;
	header.size = 0;
	header.opcode = XTION_OPCODE_GET_SERIAL_NUMBER;
	header.id = 1;

	ret = xtion_control(xtion, (u8*)&header, sizeof(header), response_buffer, &response_size);

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

int xtion_get_cmos_presets(struct xtion *xtion, unsigned int cmos, struct XtionCmosMode *modes, unsigned int num_modes)
{
	u8 response_buffer[sizeof(struct XtionReplyHeader) + 20 * 32];
	u16 response_size = sizeof(response_buffer);
	struct XtionGetCmosModesRequest req;
	int ret;
	struct XtionReplyHeader* reply = (struct XtionReplyHeader*)response_buffer;
	unsigned int size;

	req.header.magic = XTION_MAGIC_HOST;
	req.header.size = 2;
	req.header.opcode = XTION_OPCODE_GET_CMOS_PRESETS;
	req.header.id = 1;
	req.cmos = cmos;

	ret = xtion_control(xtion, (u8*)&req, sizeof(req), response_buffer, &response_size);

	if(ret < 0) {
		dev_err(&xtion->dev->dev, "Could not query CMOS presets: %d\n", ret);
		return ret;
	}

	if(response_size < sizeof(struct XtionReplyHeader) || reply->header.magic != XTION_MAGIC_DEV) {
		dev_err(&xtion->dev->dev, "Invalid response (size %d, magic 0x%X)\n", response_size, reply->header.magic);
		return -EIO;
	}

	size = response_size - sizeof(struct XtionReplyHeader);
	size = size / sizeof(struct XtionCmosMode);
	size = min(num_modes, size);

	memcpy(modes, response_buffer + sizeof(struct XtionReplyHeader), size * sizeof(struct XtionCmosMode));

	return size;
}

int xtion_set_param(struct xtion *xtion, u16 parameter, u16 value)
{
	struct XtionSetParamRequest packet;
	u8 response_buffer[sizeof(struct XtionReplyHeader) + 20];
	u16 response_size = sizeof(response_buffer);
	struct XtionReplyHeader *reply = (struct XtionReplyHeader*)response_buffer;
	int rc;

	packet.header.magic = XTION_MAGIC_HOST;
	packet.header.opcode = XTION_OPCODE_SET_PARAM;
	packet.header.size = 4;

	packet.param = parameter;
	packet.value = value;

	rc = xtion_control(xtion, (u8*)&packet, sizeof(packet), response_buffer, &response_size);
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

int xtion_reset(struct xtion *xtion)
{
	struct XtionSetModeRequest packet;
	u8 response_buffer[256];
	u16 response_size = sizeof(response_buffer);
	int rc;
	
	packet.header.magic = XTION_MAGIC_HOST;
	packet.header.opcode = XTION_OPCODE_SET_MODE;
	packet.header.size = 4;
	
	packet.mode = 3;
	
	rc = xtion_control(xtion, (u8*)&packet, sizeof(packet), response_buffer, &response_size);
	if(rc < 0) {
		dev_err(&xtion->dev->dev, "Could not set mode: %d\n", rc);
		return rc;
	}
	
	return 0;
}
