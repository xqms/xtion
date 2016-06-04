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
#include <media/v4l2-ctrls.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-v4l2.h>

#include "protocol.h"

#define DRIVER_AUTHOR "Max Schwarz <max.schwarz@online.de>"
#define DRIVER_DESC "ASUS xtion kernel driver"

#define VENDOR_ID             0x1d27   /* ASUS */
#define PRODUCT_ID_ASUS       0x0601 /* xtion with new firmware */
#define PRODUCT_ID_PRIMESENSE 0x0609 /* PrimeSense with new firmware */

#define SERIAL_NUMBER_MAX_LEN 31

#define XTION_NUM_URBS 16
// #define XTION_URB_SIZE (6 * 20480UL)
// #define XTION_URB_SIZE 81920
#define XTION_URB_SIZE 20480

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
	u16 start_id;
	u16 end_id;
	unsigned int pixel_size;

	u8 cmos_index;

	u16 settings_base;
	u16 endpoint_register;
	u16 endpoint_mode;
	u16 image_format;

	unsigned int bulk_urb_size;

	unsigned int buffer_size;

	void (*handle_start)(struct xtion_endpoint* endp);
	void (*handle_data)(struct xtion_endpoint* endp, const u8* data, unsigned int size);
	void (*handle_end)(struct xtion_endpoint* endp);

	int (*uncompress)(struct xtion_endpoint *endp, struct xtion_buffer *buf);

	int (*setup_modes)(struct xtion_endpoint *endp);

	int (*enable_streaming)(struct xtion_endpoint* endp);
};

/**
 * @resolution Resolution code, see protocol.h
 * @fps_bitset A set bit indicates that the corresponding FPS number is
 *    available
 */
struct xtion_framesize
{
	unsigned int resolution;
	u64 fps_bitset;
};

struct xtion_endpoint
{
	struct xtion *xtion;
	const struct xtion_endpoint_config *config;

	struct video_device video;
	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_pix_format pix_fmt;
	int resolution;
	u16 fps;

	/* Available image modes */
	struct xtion_framesize framesizes[10];
	unsigned int num_framesizes;

	/* Image buffers */
	struct vb2_queue vb2;
	struct mutex vb2_lock;
	struct xtion_buffer *active_buffer;

	/* USB buffers */
	struct list_head avail_bufs;
	spinlock_t buf_lock;
	struct urb *urbs[XTION_NUM_URBS];
	u8 *transfer_buffers[XTION_NUM_URBS];

	/* Packet parser */
	int packet_state;
	unsigned int packet_off;
	struct XtionSensorReplyHeader packet_header;
	unsigned int packet_data_size;
	unsigned int packet_id;
	unsigned int packet_corrupt;
	unsigned int packet_pad_start;
	unsigned int packet_pad_end;
	u32 packet_timestamp;
	struct timeval packet_system_timestamp;
	u32 frame_id;
};

struct xtion_depth
{
	struct xtion_endpoint endp;

	u8 frame_buffer[640*480*11/8+1];
	u8 frame_bytes;

	u8 temp_buffer[4096];
	u16 temp_bytes;

	u16* lut;
};

struct xtion_color
{
	struct xtion_endpoint endp;

	u32 current_channel;
	u32 current_channel_idx;
	u32 last_full_values[3];

	u32 stashed_nibble;
	u32 open_nibbles;

	u32 line_count;
};

struct xtion
{
	struct usb_device *dev;
	struct usb_interface *interface;
	struct XtionVersion version;
	struct XtionFixedParams fixed;
  struct XtionAlgorithmParams algorithm_params;
	char serial_number[SERIAL_NUMBER_MAX_LEN+1];
	struct v4l2_device v4l2_dev;

	unsigned int flags;

	u16 message_id;

	struct xtion_color color;
	struct xtion_depth depth;

	struct mutex control_mutex;
};

struct xtion_buffer
{
	struct vb2_v4l2_buffer vb;
	unsigned long pos;
	struct list_head list;
};

struct xtion_depth_buffer
{
	struct xtion_buffer xbuf;
	u8 frame_buffer[640*480*11/8];
	size_t frame_bytes;
};

#endif
