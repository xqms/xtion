/*
 * ASUS xtion depth channel
 */

#include "xtion-depth.h"
#include "xtion-endpoint.h"
#include "xtion-control.h"
#include "xtion-math-emu.h"

#include <linux/slab.h>

/* defined in xtion-depth-accel.s */
extern void xtion_depth_unpack_AVX2(const u8 *input, const u16 *lut, u16 *output, u32 size);

void (*xtion_depth_unpack)(const u8 *input, const u16 *lut, u16 *output, u32 size);

#define INPUT_ELEMENT_SIZE 11

enum XtionDepthControls
{
	XTION_DEPTH_CTRL_CLOSE_RANGE = V4L2_CID_USER_BASE,
};

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

#define TAKE_BITS(inp, count, offset) (((inp) & (((1 << count)-1) << offset)) >> offset)
static void xtion_depth_unpack_generic(const u8 *input, const u16 *lut, u16 *output, u32 size)
{
	unsigned int i, j;
	for (i = 0; i < size; ++i) {
		u16 unpacked[8] = {
			(TAKE_BITS(input[0], 8, 0) << 3) | TAKE_BITS(input[1], 3, 5),
			(TAKE_BITS(input[1], 5, 0) << 6) | TAKE_BITS(input[2], 6, 2),
			(TAKE_BITS(input[2], 2, 0) << 9) | (TAKE_BITS(input[3], 8, 0) << 1) | TAKE_BITS(input[4], 1, 7),
			(TAKE_BITS(input[4], 7, 0) << 4) | TAKE_BITS(input[5], 4, 4),
			(TAKE_BITS(input[5], 4, 0) << 7) | TAKE_BITS(input[6], 7, 1),
			(TAKE_BITS(input[6], 1, 0) << 10) | (TAKE_BITS(input[7], 8, 0) << 2) | TAKE_BITS(input[8], 2, 6),
			(TAKE_BITS(input[8], 6, 0) << 5) | TAKE_BITS(input[9], 5, 3),
			(TAKE_BITS(input[9], 3, 0) << 8) | TAKE_BITS(input[10], 8, 0)
		};

		for (j = 0; j < 8; ++j)
			output[j] = lut[unpacked[j]];

		input += 11;
		output += 8;
	}
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

		xtion_depth_unpack(src, depth->lut, (u16*)wptr, num_elements);
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

/* Depth channel is special: reported modes are for IR images. So override
 * them with fixed modes. */
static int depth_setup_modes(struct xtion_endpoint *endp)
{
	endp->num_framesizes = 2;

	endp->framesizes[0].resolution = 0;
	endp->framesizes[0].fps_bitset = (1ULL << 30) | (1ULL << 60);

	endp->framesizes[1].resolution = 1;
	endp->framesizes[1].fps_bitset = (1ULL << 30);

	return 0;
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

	.cmos_index      = 1,

	.settings_base   = XTION_P_DEPTH_BASE,
	.endpoint_register = XTION_P_GENERAL_STREAM1_MODE,
	.endpoint_mode     = XTION_VIDEO_STREAM_DEPTH,
	.image_format     = XTION_DEPTH_FORMAT_UNC_11_BIT,

	.handle_start    = depth_start,
	.handle_data     = depth_data,
	.handle_end      = depth_end,
	.uncompress      = depth_uncompress,
	.setup_modes     = depth_setup_modes
};

static int xtion_depth_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct xtion_endpoint* endp = container_of(ctrl->handler, struct xtion_endpoint, ctrl_handler);

	switch(ctrl->id) {
	case XTION_DEPTH_CTRL_CLOSE_RANGE:
		return xtion_set_param(endp->xtion, XTION_P_CLOSE_RANGE, ctrl->val);
	}

	return -EINVAL;
}

static const struct v4l2_ctrl_ops xtion_depth_ctrl_ops = {
	.s_ctrl = xtion_depth_s_ctrl,
};

static const struct v4l2_ctrl_config ctrl_close_range = {
	.ops = &xtion_depth_ctrl_ops,
	.id = XTION_DEPTH_CTRL_CLOSE_RANGE,
	.name = "Close Range",
	.type = V4L2_CTRL_TYPE_BOOLEAN,
	.min = 0,
	.max = 1,
	.step = 1,
	.def = 0,
};

void xtion_generate_lut(struct xtion *xtion, u16* plut[])
{

	uint16_t max_depth;
	size_t shift;
	int tmp1, tmp2, depth;

	float32 dcmos_emitter_distance_f32, zero_plane_pixel_size_f32, reference_distance_f32;
	float32 aa, bb1, bb11, bb2, bb22, bb3;

	*plut = (u16*)kzalloc((MAX_SHIFT_VALUE + 10) * sizeof(u16), GFP_KERNEL);

	dcmos_emitter_distance_f32.float_ = xtion->fixed.dcmos_emitter_distance;
	zero_plane_pixel_size_f32.float_ = xtion->fixed.reference_pixel_size;
	reference_distance_f32.float_ = xtion->fixed.reference_distance;

	aa.uint32_ = u2f(8 * PARAM_COEFF * SHIFT_SCALE);
	mul_f32(&reference_distance_f32, &aa, &aa);
	mul_f32(&dcmos_emitter_distance_f32, &aa, &aa);

	bb2.uint32_ = u2f(8 * PARAM_COEFF);
	mul_f32(&dcmos_emitter_distance_f32, &bb2, &bb22);

	tmp1 = (int) PARAM_COEFF * (8 * xtion->algorithm_params.const_shift + 3);

	max_depth = xtion_min(MAX_DEPTH_VALUE, DEPTH_MAX_CUTOFF);

	for (shift = 1; shift < MAX_SHIFT_VALUE; shift++)
	{
		tmp2 = 8 * (int) PIXEL_SIZE_FACTOR * shift;

		if (tmp1 < tmp2)
		{
			bb1.uint32_ = u2f((uint32_t)(tmp2 - tmp1));
			bb1.sign = 1;
		}
		else
		{
			bb1.uint32_ = u2f((uint32_t)(tmp1 - tmp2));
		}

		mul_f32(&zero_plane_pixel_size_f32, &bb1, &bb11);
		add_f32(&bb11, &bb22, &bb3);
		depth = div_f32(&aa, &bb3);

		if (depth > (int) DEPTH_MIN_CUTOFF && depth < max_depth)
			(*plut)[shift] = (u16) depth;
	}

}

int xtion_depth_init(struct xtion_depth *depth, struct xtion *xtion)
{
	int rc;

#if CONFIG_AS_AVX2
	if (boot_cpu_has(X86_FEATURE_AVX2))
		xtion_depth_unpack = xtion_depth_unpack_AVX2;
	else
#endif
	{
		xtion_depth_unpack = xtion_depth_unpack_generic;
	}

	rc = xtion_endpoint_init(&depth->endp, xtion, &xtion_depth_endpoint_config);
	if(rc != 0)
		return rc;

	xtion_generate_lut(xtion, &depth->lut);

	v4l2_ctrl_new_custom(&depth->endp.ctrl_handler, &ctrl_close_range, NULL);

	v4l2_ctrl_handler_setup(&depth->endp.ctrl_handler);

	return 0;
}

void xtion_depth_release(struct xtion_depth *depth)
{
	xtion_endpoint_release(&depth->endp);
	kfree(depth->lut);
}

