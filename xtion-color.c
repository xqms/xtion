/*
 * ASUS xtion color channel
 */

#include "xtion-color.h"
#include "xtion-endpoint.h"

struct framesize
{
	unsigned int width;
	unsigned int height;
	u16 code;
};

static const struct framesize frame_sizes[] = {
	{  320,  240,  0 },
	{  640,  480,  1 },
	{ 1280, 1024,  2 },
	{ 1600, 1200,  3 },
	{  160,  120,  4 },
	{  176,  144,  5 },
	{  432,  240,  6 },
	{  352,  288,  7 },
	{  640,  360,  8 },
	{  864,  480,  9 },
	{  800,  448, 10 },
	{  800,  600, 11 },
	{ 1024,  576, 12 },
	{  960,  720, 13 },
	{ 1280,  720, 14 },
	{ 1280,  960, 15 }
};

static inline struct xtion_color *endp_color(struct xtion_endpoint *endp)
{ return container_of(endp, struct xtion_color, endp); }

static void color_start(struct xtion_endpoint* endp)
{
	struct xtion_color *color = endp_color(endp);

	if(!endp->active_buffer) {
		endp->active_buffer = xtion_endpoint_get_next_buf(endp);
		if(!endp->active_buffer)
			return;
	}

	endp->active_buffer->pos = 0;

	color->last_full_values[0] = 0;
	color->last_full_values[1] = 0;
	color->last_full_values[2] = 0;
	color->open_nibbles = 0;
	color->current_channel_idx = 0;
	color->current_channel = 0;
	color->line_count = 0;
}

static unsigned int channel_map[4] = {0, 1, 2, 1}; // UYVY

static inline void color_put_byte(struct xtion_color* color, struct xtion_buffer *buffer, u8 val) {
	u8 *vaddr = vb2_plane_vaddr(&buffer->vb, 0);

	if(buffer->pos >= vb2_plane_size(&buffer->vb, 0)) {
		dev_warn(&color->endp.xtion->dev->dev, "buffer overflow");
		return;
	}

	vaddr[buffer->pos] = val;
	buffer->pos++;
	color->last_full_values[color->current_channel] = val;
	color->current_channel_idx = (color->current_channel_idx+1) % 4;
	color->current_channel = channel_map[color->current_channel_idx];

	if(++color->line_count == 640*2) {
		color->last_full_values[0] = 0;
		color->last_full_values[1] = 0;
		color->last_full_values[2] = 0;
		color->line_count = 0;
	}
}

static inline int color_unpack_nibble(struct xtion_color *color, struct xtion_buffer *buffer, u32 nibble) {
	if(nibble < 0xd) {
		color_put_byte(color, buffer, color->last_full_values[color->current_channel] + (__s8)(nibble - 6));
	} else if(nibble == 0xf) {
		return 1;
	}

	return 0;
}

static void color_unpack(struct xtion_color *color, const u8 *data, unsigned int size)
{
	struct xtion_buffer *buffer = color->endp.active_buffer;
	u32 c = *data;
	u32 temp;

	if(color->open_nibbles == 1) {
		/* The high nibble of the first input byte belongs to the last output byte */
		temp = (c >> 4) | color->stashed_nibble;
		color_put_byte(color, buffer, temp);

		/* Process low nibble */
		if(color_unpack_nibble(color, buffer, c & 0xF)) {
			if(size == 1) {
				color->open_nibbles = 2;
				return;
			}

			/* Take one full byte */
			data++;
			size--;

			color_put_byte(color, buffer, *data);
		}

		data++;
		size--;
	} else if(color->open_nibbles == 2) {
		/* Take one full byte */
		color_put_byte(color, buffer, c);
		data++;
		size--;
	}

	while(size > 2) {
		c = *data;

		/* Process high nibble */
		if(color_unpack_nibble(color, buffer, c >> 4)) {
			/* Take one full byte */
			temp = (c & 0xF) << 4;
			data++;
			c = *data;
			temp |= (c >> 4);
			size--;

			color_put_byte(color, buffer, temp);
		}

		/* Process low nibble */
		if(color_unpack_nibble(color, buffer, c & 0xF)) {
			/* Take one full byte */
			data++;
			size--;

			color_put_byte(color, buffer, *data);
		}

		data++;
		size--;
	}

	/* Be careful with the last two bytes, we might not have enough data
	 * to process them fully. */
	if(size == 2) {
		c = *data;

		/* Process high nibble */
		if(color_unpack_nibble(color, buffer, c >> 4)) {
			/* Take one full byte */
			temp = (c & 0xF) << 4;
			data++;
			c = *data;
			temp |= (c >> 4);
			size--;

			color_put_byte(color, buffer, temp);
		}

		/* Process low nibble */
		if(color_unpack_nibble(color, buffer, c & 0xF)) {
			if(size == 1) {
				color->open_nibbles = 2;
				return;
			}

			/* Take one full byte */
			data++;
			size--;

			color_put_byte(color, buffer, *data);
		}

		data++;
		size--;
	}

	if(size == 1) {
		c = *data;

		/* Process high nibble */
		if(color_unpack_nibble(color, buffer, c >> 4)) {
			/* We want to take a full byte, but we do not have the second nibble */
			color->stashed_nibble = (c & 0xF) << 4;
			color->open_nibbles = 1;
			return;
		}

		/* Process low nibble */
		if(color_unpack_nibble(color, buffer, c & 0xF)) {
			/* We want to take a full byte, but this is the last byte */
			color->open_nibbles = 2;
			return;
		}
	}

	color->open_nibbles = 0;
}

static void color_data(struct xtion_endpoint* endp, const u8* data, unsigned int size)
{
	struct xtion_color *color = endp_color(endp);
	u8* vaddr;

	if(!endp->active_buffer)
		return;

	vaddr = vb2_plane_vaddr(&endp->active_buffer->vb, 0);
	if(!vaddr)
		return;

	if(size != 0)
		color_unpack(color, data, size);
}

static void color_end(struct xtion_endpoint *endp)
{
	if(!endp->active_buffer)
		return;

	endp->active_buffer->vb.v4l2_buf.bytesused = endp->active_buffer->pos;
	endp->active_buffer->vb.v4l2_buf.timestamp = endp->packet_system_timestamp;
	endp->active_buffer->vb.v4l2_buf.sequence = endp->frame_id;

	vb2_set_plane_payload(&endp->active_buffer->vb, 0, endp->active_buffer->pos);
	vb2_buffer_done(&endp->active_buffer->vb, VB2_BUF_STATE_DONE);
	endp->active_buffer = 0;
}

static int color_enumerate_sizes(struct xtion_endpoint *endp, struct v4l2_frmsizeenum *size)
{
	if(size->index >= ARRAY_SIZE(frame_sizes))
		return -EINVAL;

	size->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	size->discrete.width = frame_sizes[size->index].width;
	size->discrete.height = frame_sizes[size->index].height;

	return 0;
}

static int color_lookup_size(struct xtion_endpoint *endp, unsigned int width, unsigned int height)
{
	int i;
	for(i = 0; i < ARRAY_SIZE(frame_sizes); ++i) {
		if(frame_sizes[i].width == width && frame_sizes[i].height == height)
			return frame_sizes[i].code;
	}

	return -EINVAL;
}

const struct xtion_endpoint_config xtion_color_endpoint_config = {
	.name            = "color",
	.addr            = 0x82,
	.start_id        = 0x8100,
	.end_id          = 0x8500,
	.pix_fmt         = V4L2_PIX_FMT_UYVY,
	.pixel_size      = 2,
	.buffer_size     = sizeof(struct xtion_buffer),

	.settings_base   = XTION_P_IMAGE_BASE,
	.endpoint_register = XTION_P_GENERAL_STREAM0_MODE,
	.endpoint_mode     = XTION_VIDEO_STREAM_COLOR,
	.image_format     = XTION_IMG_FORMAT_YUV422,

	.bulk_urb_size   = 20480,

	.handle_start    = color_start,
	.handle_data     = color_data,
	.handle_end      = color_end,
	.enumerate_sizes = color_enumerate_sizes,
	.lookup_size     = color_lookup_size
};

int xtion_color_init(struct xtion_color *color, struct xtion *xtion)
{
	int rc;

	rc = xtion_endpoint_init(&color->endp, xtion, &xtion_color_endpoint_config);
	if(rc != 0)
		return rc;

	return 0;
}

void xtion_color_release(struct xtion_color* color)
{
	xtion_endpoint_release(&color->endp);
}
