/*
 * ASUS xtion kernel driver
 *
 * Author: Max Schwarz <max.schwarz@online.de>
 */

#ifndef XTION_H
#define XTION_H

#include <linux/usb.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-device.h>
#include <media/videobuf2-core.h>

#include "protocol.h"

#define DRIVER_AUTHOR "Max Schwarz <max.schwarz@online.de>"
#define DRIVER_DESC "ASUS xtion kernel driver"

#define VENDOR_ID 0x1d27   /* ASUS */
#define PRODUCT_ID 0x0601  /* xtion with new firmware */

#define SERIAL_NUMBER_MAX_LEN 31

#define XTION_NUM_URBS 8
#define XTION_URB_SIZE 81920

struct xtion;
struct xtion_endpoint;

enum PacketState
{
	XTION_PS_MAGIC1,
	XTION_PS_MAGIC2,
	XTION_PS_HEADER,
	XTION_PS_DATA
};

struct xtion_endpoint_config
{
	unsigned int addr;
	__u16 start_id;
	__u16 end_id;

	void (*handle_start)(struct xtion_endpoint* endp);
	void (*handle_data)(struct xtion_endpoint* endp, const __u8* data, unsigned int size);
	void (*handle_end)(struct xtion_endpoint* endp);
};

struct xtion_endpoint
{
	struct xtion *xtion;
	const struct xtion_endpoint_config *config;

	struct video_device video;
	struct v4l2_pix_format pix_fmt;

	/* Image buffers */
	struct vb2_queue vb2;

	/* USB buffers */
	struct list_head avail_bufs;
	spinlock_t buf_lock;
	struct urb *urbs[XTION_NUM_URBS];
	__u8 *transfer_buffers[XTION_NUM_URBS];

	/* Packet parser */
	int packet_state;
	unsigned int packet_off;
	struct XtionSensorReplyHeader packet_header;
	unsigned int packet_data_size;
	unsigned int packet_id;
	unsigned int packet_corrupt;
	unsigned int packet_pad_start;
	unsigned int packet_pad_end;
};

struct xtion
{
	struct usb_device* dev;
	struct XtionVersion version;
	struct XtionFixedParams fixed;
	char serial_number[SERIAL_NUMBER_MAX_LEN+1];
	struct v4l2_device v4l2_dev;

	__u16 message_id;

	struct xtion_endpoint color;
};

struct xtion_buffer
{
	struct vb2_buffer vb;
	unsigned long pos;
	struct list_head list;
};

#endif
