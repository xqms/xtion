/*
 * ASUS xtion depth channel
 */

#include "xtion-depth.h"
#include "xtion-endpoint.h"

#include "xtion-depth-lut.h"

/* defined in xtion-depth-accel.s */
extern void xtion_depth_unpack_AVX2(const u8 *input, const u16 *lut, u16 *output, u32 size);

#define INPUT_ELEMENT_SIZE 11

struct framesize
{
	unsigned int width;
	unsigned int height;
	u16 code;
};

static const struct framesize frame_sizes[] = {
	{  320,  240,  0 },
	{  640,  480,  1 },
};

static inline struct xtion_depth *endp_depth(struct xtion_endpoint *endp)
{ return container_of(endp, struct xtion_depth, endp); }

static inline struct xtion_depth_buffer *depth_buf(struct xtion_buffer* buf)
{ return container_of(buf, struct xtion_depth_buffer, xbuf); }

static void depth_start(struct xtion_endpoint* endp)
{
	struct xtion_depth_buffer *dbuf;
	u8 *vaddr;
	size_t length;

	if(!endp->active_buffer) {
		endp->active_buffer = xtion_endpoint_get_next_buf(endp);
		if(!endp->active_buffer)
			return;
	}

	dbuf = depth_buf(endp->active_buffer);

	vaddr = vb2_plane_vaddr(&dbuf->xbuf.vb, 0);
	length = vb2_plane_size(&dbuf->xbuf.vb, 0);
	if(!vaddr)
		return;

	dbuf->xbuf.pos = 2 * endp->packet_pad_start;

	if(dbuf->xbuf.pos > length)
		return;

	memset(vaddr, 0, dbuf->xbuf.pos);

	dbuf->frame_bytes = 0;
}

static inline size_t depth_unpack(struct xtion_depth *depth, struct xtion_buffer *buf, const u8* src, size_t size)
{
	size_t num_elements = size / INPUT_ELEMENT_SIZE;
	u8* vaddr = vb2_plane_vaddr(&buf->vb, 0);
	u8* wptr = vaddr + buf->pos;
	size_t num_bytes;

	if(!vaddr)
		return 0;

	if(num_elements != 0) {
		num_bytes = num_elements * 8 * sizeof(u16);
		if(buf->pos + num_bytes > vb2_plane_size(&buf->vb, 0)) {
			dev_err(&depth->endp.xtion->dev->dev, "depth buffer overflow: %lu\n", buf->pos + num_bytes);
			return num_elements * INPUT_ELEMENT_SIZE;
		}

		xtion_depth_unpack_AVX2(src, depth->lut, (u16*)wptr, num_elements);
		buf->pos += num_bytes;
	}

	return num_elements * INPUT_ELEMENT_SIZE;
}

static void depth_data(struct xtion_endpoint* endp, const u8* data, unsigned int size)
{
	struct xtion_depth_buffer* dbuf = depth_buf(endp->active_buffer);
	size_t bytes;

	if(!dbuf)
		return;

	if(size == 0)
		return;

	bytes = min_t(size_t, size, sizeof(dbuf->frame_buffer) - dbuf->frame_bytes);

	memcpy(dbuf->frame_buffer + dbuf->frame_bytes, data, bytes);
	dbuf->frame_bytes += bytes;
}

static void depth_end(struct xtion_endpoint *endp)
{
	struct xtion_buffer *buffer = endp->active_buffer;
	if(!buffer)
		return;

	if(vb2_is_streaming(&endp->xtion->color.endp.vb2)) {
		/* Use timestamp & seq info from color frame */
		buffer->vb.v4l2_buf.timestamp = endp->xtion->color.endp.packet_system_timestamp;
		buffer->vb.v4l2_buf.sequence = endp->xtion->color.endp.frame_id;
	} else {
		buffer->vb.v4l2_buf.timestamp = endp->packet_system_timestamp;
		buffer->vb.v4l2_buf.sequence = endp->frame_id;
	}

	vb2_buffer_done(&buffer->vb, VB2_BUF_STATE_DONE);
	endp->active_buffer = 0;
}

static int depth_uncompress(struct xtion_endpoint *endp, struct xtion_buffer *buf)
{
	struct xtion_depth *depth = endp_depth(endp);
	struct xtion_depth_buffer *dbuf = depth_buf(buf);

	size_t pad = endp->packet_pad_end * 2;

	depth_unpack(depth, buf, dbuf->frame_buffer, dbuf->frame_bytes);
	if(buf->pos + pad > endp->pix_fmt.sizeimage) {
		dev_err(&endp->xtion->dev->dev, "depth buffer overflow (padding)\n");
		return 1;
	}
	memset(vb2_plane_vaddr(&buf->vb, 0), 0, pad);
	buf->pos += pad;
	buf->vb.v4l2_buf.bytesused = buf->pos;
	vb2_set_plane_payload(&buf->vb, 0, buf->pos);

	return 0;
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
	.buffer_size     = sizeof(struct xtion_depth_buffer),

	.settings_base   = XTION_P_DEPTH_BASE,
	.endpoint_register = XTION_P_GENERAL_STREAM1_MODE,
	.endpoint_mode     = XTION_VIDEO_STREAM_DEPTH,
	.image_format     = XTION_DEPTH_FORMAT_UNC_11_BIT,

	.handle_start    = depth_start,
	.handle_data     = depth_data,
	.handle_end      = depth_end,
	.enumerate_sizes = depth_enumerate_sizes,
	.lookup_size     = depth_lookup_size,
	.uncompress      = depth_uncompress
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

