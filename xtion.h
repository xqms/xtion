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

#define XTION_NUM_URBS 32
#define XTION_URB_SIZE (8 * 20480)
// #define XTION_URB_SIZE 81920

#define XTION_FLAG_ISOC (1 << 0)

struct xtion;
struct xtion_endpoint;
struct xtion_buffer;

enum PacketState
{
	XTION_PS_MAGIC1,
	XTION_PS_MAGIC2,
	XTION_PS_HEADER,
	XTION_PS_DATA
};

struct xtion_endpoint_config
{
	char name[64];
	unsigned int addr;
	unsigned int pix_fmt;
	__u16 start_id;
	__u16 end_id;
	unsigned int pixel_size;

	__u16 settings_base;
	__u16 endpoint_register;
	__u16 endpoint_mode;
	__u16 image_format;

	unsigned int bulk_urb_size;

	unsigned int buffer_size;

	void (*handle_start)(struct xtion_endpoint* endp);
	void (*handle_data)(struct xtion_endpoint* endp, const __u8* data, unsigned int size);
	void (*handle_end)(struct xtion_endpoint* endp);

	int (*enumerate_sizes)(struct xtion_endpoint *endp, struct v4l2_frmsizeenum *framesize);
	int (*enumerate_rates)(struct xtion_endpoint *endp, struct v4l2_frmivalenum *interval);
	int (*lookup_size)(struct xtion_endpoint *endp, unsigned int width, unsigned int height);

	int (*uncompress)(struct xtion_endpoint *endp, struct xtion_buffer *buf);
};

struct xtion_endpoint
{
	struct xtion *xtion;
	const struct xtion_endpoint_config *config;

	struct video_device video;
	struct v4l2_pix_format pix_fmt;
	__u16 fps;

	/* Image buffers */
	struct vb2_queue vb2;
	struct mutex vb2_lock;
	struct xtion_buffer *active_buffer;

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
	__u32 packet_timestamp;
	struct timeval packet_system_timestamp;
	__u32 frame_id;
};

struct xtion_depth
{
	struct xtion_endpoint endp;

	__u8 frame_buffer[640*480*11/8+1];
	__u8 frame_bytes;

	__u8 temp_buffer[4096];
	__u16 temp_bytes;

	const __u16* lut;
};

struct xtion_color
{
	struct xtion_endpoint endp;

	__u32 current_channel;
	__u32 current_channel_idx;
	__u32 last_full_values[3];

	__u32 stashed_nibble;
	__u32 open_nibbles;

	__u32 line_count;
};

struct xtion
{
	struct usb_device* dev;
	struct XtionVersion version;
	struct XtionFixedParams fixed;
	char serial_number[SERIAL_NUMBER_MAX_LEN+1];
	struct v4l2_device v4l2_dev;

	unsigned int flags;

	__u16 message_id;

	struct xtion_color color;
	struct xtion_depth depth;

	struct mutex control_mutex;
};

struct xtion_buffer
{
	struct vb2_buffer vb;
	unsigned long pos;
	struct list_head list;
};

struct xtion_depth_buffer
{
	struct xtion_buffer xbuf;
	__u8 frame_buffer[640*480*11/9];
	size_t frame_bytes;
};

#endif
