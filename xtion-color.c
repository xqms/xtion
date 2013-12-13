/*
 * ASUS xtion color channel
 */

#include "xtion-color.h"

struct framesize
{
	unsigned int width;
	unsigned int height;
	__u16 code;
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

static void color_start(struct xtion_endpoint* endp)
{
	unsigned long flags = 0;

	if(!endp->active_buffer) {
		/* Find free buffer */
		spin_lock_irqsave(&endp->buf_lock, flags);
		if (!list_empty(&endp->avail_bufs)) {
				endp->active_buffer = list_first_entry(&endp->avail_bufs, struct xtion_buffer, list);
				list_del(&endp->active_buffer->list);
		}
		spin_unlock_irqrestore(&endp->buf_lock, flags);
	}

	if(!endp->active_buffer)
		return;

// 	dev_info(&endp->xtion->dev->dev, "start\n");
	endp->active_buffer->pos = 0;
}

static void color_data(struct xtion_endpoint* endp, const __u8* data, unsigned int size)
{
	__u8* vaddr;

	if(!endp->active_buffer) {
		dev_err(&endp->xtion->dev->dev, "data without buffer\n");
		return;
	}

	vaddr = vb2_plane_vaddr(&endp->active_buffer->vb, 0);
	if(!vaddr) {
// 		dev_err(&endp->xtion->dev->dev, "no vaddr\n");
		return;
	}

	if(endp->active_buffer->pos + size > endp->pix_fmt.sizeimage) {
		dev_err(&endp->xtion->dev->dev, "buffer overflow: %lu + %d > %u\n", endp->active_buffer->pos, size, endp->pix_fmt.sizeimage);
		return;
	}

	memcpy(vaddr + endp->active_buffer->pos, data, size);
	endp->active_buffer->pos += size;
}

static void color_end(struct xtion_endpoint *endp)
{
	if(!endp->active_buffer)
		return;

	endp->active_buffer->vb.v4l2_buf.bytesused = endp->active_buffer->pos;

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

	.settings_base   = XTION_P_IMAGE_BASE,
	.endpoint_register = XTION_P_GENERAL_STREAM0_MODE,
	.endpoint_mode     = XTION_VIDEO_STREAM_COLOR,
	.image_format     = XTION_IMG_FORMAT_UNC_YUV422,

	.handle_start    = color_start,
	.handle_data     = color_data,
	.handle_end      = color_end,
	.enumerate_sizes = color_enumerate_sizes,
	.lookup_size     = color_lookup_size
};
