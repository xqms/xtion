/*
 * ASUS xtion depth channel
 */

#include "xtion-depth.h"
#include "xtion-endpoint.h"

#include "xtion-depth-lut.h"

/* defined in xtion-depth-accel.s */
extern void xtion_depth_unpack_AVX2(const __u8 *input, const __u16 *lut, __u16 *output, __u32 size);

#define INPUT_ELEMENT_SIZE 11

struct framesize
{
	unsigned int width;
	unsigned int height;
	__u16 code;
};

static const struct framesize frame_sizes[] = {
	{  320,  240,  0 },
	{  640,  480,  1 },
};

static inline struct xtion_depth *endp_depth(struct xtion_endpoint *endp)
{ return container_of(endp, struct xtion_depth, endp); }

static void depth_start(struct xtion_endpoint* endp)
{
	struct xtion_depth *depth = endp_depth(endp);
	unsigned long flags = 0;
	__u8 *vaddr;

	dev_info(&endp->xtion->dev->dev, "depth_start\n");

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

	vaddr = vb2_plane_vaddr(&endp->active_buffer->vb, 0);
	if(!vaddr)
		return;

	endp->active_buffer->pos = 2 * endp->packet_pad_start;

	if(endp->active_buffer->pos > endp->pix_fmt.sizeimage)
		return;

	memset(vaddr, 0, endp->active_buffer->pos);

	depth->temp_bytes = 0;
}

static inline size_t depth_unpack(struct xtion_depth *depth, struct xtion_buffer *buf, const __u8* src, size_t size)
{
	size_t num_elements = size / INPUT_ELEMENT_SIZE;
	__u8* vaddr = vb2_plane_vaddr(&buf->vb, 0);
	__u8* wptr = vaddr + buf->pos;

	if(!vaddr)
		return 0;

	if(num_elements != 0) {
		xtion_depth_unpack_AVX2(src, depth->lut, (__u16*)wptr, num_elements);
		wptr += 2 * 8 * num_elements;
		buf->pos += 2 * 8 * num_elements;
	}

	return num_elements * INPUT_ELEMENT_SIZE;
}

static void depth_data(struct xtion_endpoint* endp, const __u8* data, unsigned int size)
{
	struct xtion_depth *depth = endp_depth(endp);
	size_t bytes;

	if(!endp->active_buffer) {
// 		dev_err(&endp->xtion->dev->dev, "data without buffer\n");
		return;
	}

// 	dev_info(&endp->xtion->dev->dev, "depth_data\n");

	if(depth->temp_bytes != 0) {
		/* Process leftover data from the last packet */
		bytes = min_t(size_t, size, INPUT_ELEMENT_SIZE - depth->temp_bytes);

		if(bytes >= INPUT_ELEMENT_SIZE || bytes >= size) {
			dev_err(&depth->endp.xtion->dev->dev, "BUG: %lu\n", bytes);
			return;
		}

		if(depth->temp_bytes + bytes > sizeof(depth->temp_buffer)) {
			dev_err(&depth->endp.xtion->dev->dev, "=============== BUG2: %u + %lu > %lu\n", depth->temp_bytes, bytes, sizeof(depth->temp_buffer));
			return;
		}
		memcpy(depth->temp_buffer + depth->temp_bytes, data, bytes);
		depth->temp_bytes += bytes;
		data += bytes;
		size -= bytes;

		if(depth->temp_bytes == INPUT_ELEMENT_SIZE) {
			depth_unpack(depth, endp->active_buffer, depth->temp_buffer, INPUT_ELEMENT_SIZE);
			depth->temp_bytes = 0;
		}
	}

	if(size == 0)
		return;

	bytes = depth_unpack(depth, endp->active_buffer, data, size);

	data += bytes;
	size -= bytes;

	/* Stash leftover bytes */
	if(depth->temp_bytes + size > sizeof(depth->temp_buffer)) {
		dev_err(&depth->endp.xtion->dev->dev, "=============== BUG3: %u + %u > %lu\n", depth->temp_bytes, size, sizeof(depth->temp_buffer));
		return;
	}

	memcpy(depth->temp_buffer, data, size);
	depth->temp_bytes = size;
}

static void depth_end(struct xtion_endpoint *endp)
{
	if(!endp->active_buffer)
		return;

	dev_info(&endp->xtion->dev->dev, "depth_end\n");

	endp->active_buffer->vb.v4l2_buf.bytesused = endp->active_buffer->pos;

	vb2_set_plane_payload(&endp->active_buffer->vb, 0, endp->active_buffer->pos);
	vb2_buffer_done(&endp->active_buffer->vb, VB2_BUF_STATE_DONE);
	endp->active_buffer = 0;
}

static int depth_enumerate_sizes(struct xtion_endpoint *endp, struct v4l2_frmsizeenum *size)
{
	if(size->index >= ARRAY_SIZE(frame_sizes))
		return -EINVAL;

	size->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	size->discrete.width = frame_sizes[size->index].width;
	size->discrete.height = frame_sizes[size->index].height;

	return 0;
}

static int depth_lookup_size(struct xtion_endpoint *endp, unsigned int width, unsigned int height)
{
	int i;
	for(i = 0; i < ARRAY_SIZE(frame_sizes); ++i) {
		if(frame_sizes[i].width == width && frame_sizes[i].height == height)
			return frame_sizes[i].code;
	}

	return -EINVAL;
}

static const struct xtion_endpoint_config xtion_depth_endpoint_config = {
	.name            = "depth",
	.addr            = 0x81,
	.start_id        = 0x7100,
	.end_id          = 0x7500,
	.pix_fmt         = v4l2_fourcc('Y', '1', '1', ' '), /* 11-bit greyscale */
	.pixel_size      = 2,
	.bulk_urb_size   = 20480 / 4,

	.settings_base   = XTION_P_DEPTH_BASE,
	.endpoint_register = XTION_P_GENERAL_STREAM1_MODE,
	.endpoint_mode     = XTION_VIDEO_STREAM_DEPTH,
	.image_format     = XTION_DEPTH_FORMAT_UNC_11_BIT,

	.handle_start    = depth_start,
	.handle_data     = depth_data,
	.handle_end      = depth_end,
	.enumerate_sizes = depth_enumerate_sizes,
	.lookup_size     = depth_lookup_size
};

int xtion_depth_init(struct xtion_depth *depth, struct xtion *xtion)
{
	int rc;

	rc = xtion_endpoint_init(&depth->endp, xtion, &xtion_depth_endpoint_config);
	if(rc != 0)
		return rc;

	depth->lut = depth_lut;

	return 0;
}

void xtion_depth_release(struct xtion_depth *depth)
{
	xtion_endpoint_release(&depth->endp);
}

