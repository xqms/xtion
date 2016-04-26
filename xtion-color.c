/*
 * ASUS xtion color channel
 */

#include "xtion-color.h"
#include "xtion-endpoint.h"
#include "xtion-control.h"

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
	u8 *vaddr = vb2_plane_vaddr(&buffer->vb.vb2_buf, 0);

	if(buffer->pos >= vb2_plane_size(&buffer->vb.vb2_buf, 0)) {
		dev_warn(&color->endp.xtion->dev->dev, "buffer overflow");
		return;
	}

	vaddr[buffer->pos] = val;
	buffer->pos++;
	color->last_full_values[color->current_channel] = val;
	color->current_channel_idx = (color->current_channel_idx+1) % 4;
	color->current_channel = channel_map[color->current_channel_idx];

	if(++color->line_count == 2*color->endp.pix_fmt.width) {
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

	vaddr = vb2_plane_vaddr(&endp->active_buffer->vb.vb2_buf, 0);
	if(!vaddr)
		return;

	if(size != 0)
		color_unpack(color, data, size);
}

static void color_end(struct xtion_endpoint *endp)
{
	if(!endp->active_buffer)
		return;

	endp->active_buffer->vb.vb2_buf.planes[0].bytesused = endp->active_buffer->pos;
	endp->active_buffer->vb.timestamp = endp->packet_system_timestamp;
	endp->active_buffer->vb.sequence = endp->frame_id;

	vb2_set_plane_payload(&endp->active_buffer->vb.vb2_buf, 0, endp->active_buffer->pos);
	vb2_buffer_done(&endp->active_buffer->vb.vb2_buf, VB2_BUF_STATE_DONE);
	endp->active_buffer = 0;
}

const struct xtion_endpoint_config xtion_color_endpoint_config = {
	.name            = "color",
	.addr            = 0x82,
	.start_id        = 0x8100,
	.end_id          = 0x8500,
	.pix_fmt         = V4L2_PIX_FMT_UYVY,
	.pixel_size      = 2,
	.buffer_size     = sizeof(struct xtion_buffer),

	.cmos_index      = 0,

	.settings_base   = XTION_P_IMAGE_BASE,
	.endpoint_register = XTION_P_GENERAL_STREAM0_MODE,
	.endpoint_mode     = XTION_VIDEO_STREAM_COLOR,
	.image_format     = XTION_IMG_FORMAT_YUV422,

	.bulk_urb_size   = 20480,

	.handle_start    = color_start,
	.handle_data     = color_data,
	.handle_end      = color_end,
};

static int xtion_color_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct xtion_endpoint *endp = container_of(ctrl->handler, struct xtion_endpoint, ctrl_handler);

	switch(ctrl->id) {
	case V4L2_CID_POWER_LINE_FREQUENCY:
		switch(ctrl->val) {
		case V4L2_CID_POWER_LINE_FREQUENCY_DISABLED:
			return xtion_set_param(endp->xtion, XTION_P_IMAGE_FLICKER, 0);
		case V4L2_CID_POWER_LINE_FREQUENCY_50HZ:
			return xtion_set_param(endp->xtion, XTION_P_IMAGE_FLICKER, 50);
		case V4L2_CID_POWER_LINE_FREQUENCY_60HZ:
			return xtion_set_param(endp->xtion, XTION_P_IMAGE_FLICKER, 60);
		default:
			return -EINVAL;
		}

		break;
	case V4L2_CID_AUTOGAIN:
		return xtion_set_param(endp->xtion, XTION_P_IMAGE_AUTO_EXPOSURE_MODE, ctrl->val);
	case V4L2_CID_GAIN:
		return xtion_set_param(endp->xtion, XTION_P_IMAGE_AGC, ctrl->val);
	case V4L2_CID_WHITE_BALANCE_TEMPERATURE:
		return xtion_set_param(endp->xtion, XTION_P_IMAGE_COLOR_TEMPERATURE, ctrl->val);
	case V4L2_CID_AUTO_WHITE_BALANCE:
		return xtion_set_param(endp->xtion, XTION_P_IMAGE_AUTO_WHITE_BALANCE_MODE, ctrl->val);
	}

	return 0;
}

static const struct v4l2_ctrl_ops xtion_color_ctrl_ops = {
	.s_ctrl = xtion_color_s_ctrl,
};

int xtion_color_init(struct xtion_color *color, struct xtion *xtion)
{
	int rc;

	rc = xtion_endpoint_init(&color->endp, xtion, &xtion_color_endpoint_config);
	if(rc != 0)
		return rc;

	v4l2_ctrl_new_std_menu(&color->endp.ctrl_handler, &xtion_color_ctrl_ops,
		V4L2_CID_POWER_LINE_FREQUENCY, V4L2_CID_POWER_LINE_FREQUENCY_60HZ,
		0, V4L2_CID_POWER_LINE_FREQUENCY_DISABLED);

	v4l2_ctrl_new_std(&color->endp.ctrl_handler, &xtion_color_ctrl_ops,
		V4L2_CID_GAIN, 0, 1500, 1, 100
	);

	v4l2_ctrl_new_std(&color->endp.ctrl_handler, &xtion_color_ctrl_ops,
		V4L2_CID_AUTOGAIN, 0, 1, 1, 1
	);

	v4l2_ctrl_new_std(&color->endp.ctrl_handler, &xtion_color_ctrl_ops,
		V4L2_CID_WHITE_BALANCE_TEMPERATURE, 0, 15000, 1, 5000
	);

	v4l2_ctrl_new_std(&color->endp.ctrl_handler, &xtion_color_ctrl_ops,
		V4L2_CID_AUTO_WHITE_BALANCE, 0, 1, 1, 1
	);

	if(color->endp.ctrl_handler.error) {
		dev_err(&color->endp.xtion->dev->dev, "could not register ctrl: %d\n", color->endp.ctrl_handler.error);
	}

	v4l2_ctrl_handler_setup(&color->endp.ctrl_handler);

	return 0;
}

void xtion_color_release(struct xtion_color* color)
{
	xtion_endpoint_release(&color->endp);
}
