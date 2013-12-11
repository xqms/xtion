/*
 * One data endpoint for ASUS Xtion Pro Live
 */

#include "xtion_endpoint.h"

#include <media/videobuf2-vmalloc.h>
#include <media/v4l2-ioctl.h>
#include <linux/usb.h>
#include <linux/slab.h>

#include "xtion_io.h"

/******************************************************************************/
/*
 * USB handling
 */

static void xtion_usb_process(struct xtion_endpoint *endp, const __u8 *data, unsigned int size)
{
	unsigned int off = 0;
	const __u8 *rptr;
	unsigned int to_read;

	while(off < size) {
		rptr = data + off;

		switch(endp->packet_state)
		{
		case XTION_PS_MAGIC1:
			if(*rptr == (XTION_MAGIC_DEV & 0xFF))
				endp->packet_state = XTION_PS_MAGIC2;
			off++;
			break;
		case XTION_PS_MAGIC2:
			if(*rptr == (XTION_MAGIC_DEV >> 8)) {
				endp->packet_state = XTION_PS_HEADER;
				endp->packet_off = 2; /* We already read the magic bytes */
				off++;
			} else {
				endp->packet_state = XTION_PS_MAGIC1;
			}
			break;
		case XTION_PS_HEADER:
			to_read = min_t(unsigned int, size - off, sizeof(struct XtionSensorReplyHeader));
			memcpy((__u8*)&endp->packet_header + endp->packet_off, rptr, to_read);
			endp->packet_off += to_read;
			off += to_read;

			if(endp->packet_off == sizeof(struct XtionSensorReplyHeader)) {
				/* header complete */

				endp->packet_data_size = ((endp->packet_header.bufSize_high << 8) | endp->packet_header.bufSize_low) - sizeof(struct XtionSensorReplyHeader);
				endp->packet_off = 0;
				endp->packet_state = XTION_PS_DATA;

				if(endp->packet_header.type == endp->config->start_id) {
					endp->packet_id = endp->packet_header.packetID;
					endp->packet_corrupt = 0;

					/* padding information is returned in the timestamp field */
					endp->packet_pad_start = endp->packet_header.timestamp >> 16;
					endp->packet_pad_end = endp->packet_header.timestamp & 0xFFFF;
				}  else {
					/* continuation, check packet ID */

					if(endp->packet_header.packetID != endp->packet_id) {
						endp->packet_corrupt = 1;
					}
				}

				endp->config->handle_start(endp);
			}
			break;
		case XTION_PS_DATA:
			to_read = min(size - off, endp->packet_data_size - endp->packet_off);

			if(!endp->packet_corrupt)
				endp->config->handle_data(endp, rptr, to_read);

			endp->packet_off += to_read;
			off += to_read;

			if(endp->packet_off == endp->packet_data_size) {
				if(endp->packet_header.type == endp->config->end_id && !endp->packet_corrupt) {
					endp->config->handle_end(endp);
				}

				endp->packet_state = XTION_PS_MAGIC1;
			}
			break;
		}
	}
}

static void xtion_usb_irq(struct urb *urb)
{
	struct xtion_endpoint *endp = urb->context;
	int rc;

	switch(urb->status)
	{
	case 0:
		break;
	case -ECONNRESET:
	case -ENOENT:
	case -ESHUTDOWN:
		return;
	default:
		dev_err(&endp->xtion->dev->dev, "Unknown URB status %d\n", urb->status);
		return;
	}

	/* process data */
	xtion_usb_process(endp, urb->transfer_buffer, urb->actual_length);

	rc = usb_submit_urb(urb, GFP_ATOMIC);
	if(rc != 0)
		dev_err(&endp->xtion->dev->dev, "URB re-submit failed with code %d\n", rc);
}

static void xtion_usb_release(struct xtion_endpoint *endp)
{
	int i;
	for(i = 0; i < XTION_NUM_URBS; ++i)
	{
		if(endp->transfer_buffers[i])
		{
			kfree(endp->transfer_buffers[i]);
			endp->transfer_buffers[i] = 0;
		}

		if(endp->urbs[i])
		{
			usb_free_urb(endp->urbs[i]);
			endp->urbs[i] = 0;
		}
	}
}

static int xtion_usb_init(struct xtion_endpoint* endp)
{
	struct urb *urb;
	int i;

	memset(endp->transfer_buffers, 0, sizeof(endp->transfer_buffers));
	memset(endp->urbs, 0, sizeof(endp->transfer_buffers));

	for(i = 0; i < XTION_NUM_URBS; ++i)
	{
		urb = usb_alloc_urb(0, GFP_KERNEL);
		if(!urb)
			return -ENOMEM;

		endp->urbs[i] = urb;
		endp->transfer_buffers[i] = kmalloc(XTION_URB_SIZE, GFP_KERNEL);

		if(!endp->transfer_buffers[i])
			goto free_buffers;

		urb->dev = endp->xtion->dev;
		urb->pipe = usb_rcvbulkpipe(endp->xtion->dev, 0x82);
		urb->transfer_buffer = endp->transfer_buffers[i];
		urb->transfer_buffer_length = XTION_URB_SIZE;
		urb->complete = xtion_usb_irq;
		urb->context = endp;
		urb->interval = 0;
		urb->start_frame = 0;
		urb->number_of_packets = 0;
	}

	return 0;
free_buffers:
	xtion_usb_release(endp);
	return -ENOMEM;
}

int xtion_enable_streaming(struct xtion_endpoint *endp)
{
	int i;
	int rc;
	struct xtion *xtion = endp->xtion;

	if(!xtion->dev)
		return -ENODEV;

	xtion_set_param(xtion, XTION_P_GENERAL_STREAM0_MODE, XTION_VIDEO_STREAM_OFF);
	xtion_set_param(xtion, XTION_P_IMAGE_FORMAT, 5); // Uncompressed YUV422
	xtion_set_param(xtion, XTION_P_IMAGE_RESOLUTION, 1); // VGA
	xtion_set_param(xtion, XTION_P_IMAGE_FPS, 30);
	xtion_set_param(xtion, XTION_P_GENERAL_STREAM0_MODE, XTION_VIDEO_STREAM_COLOR);

	// Submit all URBs initially
	for(i = 0; i < XTION_NUM_URBS; ++i) {
		rc = usb_submit_urb(endp->urbs[i], GFP_KERNEL);
		if(rc != 0) {
			dev_err(&xtion->dev->dev, "Could not submit URB: %d\n", rc);
			return 1;
		}
	}

	return 0;
}

static int xtion_disable_streaming(struct xtion_endpoint *endp)
{
	int i;

	// Kill all pending URBs
	for(i = 0; i < XTION_NUM_URBS; ++i) {
		if(endp->urbs[i]) {
			usb_kill_urb(endp->urbs[i]);
		}
	}

	// End disable streaming
	return xtion_set_param(endp->xtion, XTION_P_GENERAL_STREAM0_MODE, XTION_VIDEO_STREAM_OFF);
}

/******************************************************************************/
/*
 * v4l2 operations
 */

static struct v4l2_file_operations xtion_v4l2_fops = {
	.owner = THIS_MODULE,
	.open = v4l2_fh_open,
	.release = v4l2_fh_release,
	.mmap = vb2_fop_mmap,
	.unlocked_ioctl = video_ioctl2
};

/******************************************************************************/
/*
 * v4l2 ioctls
 */

static int xtion_vidioc_querycap(struct file *fp, void *priv, struct v4l2_capability *cap)
{
	struct xtion_endpoint *endp = video_drvdata(fp);

	strcpy(cap->driver, "xtion");
	strncpy(cap->card, endp->xtion->serial_number, sizeof(cap->card));

	usb_make_path(endp->xtion->dev, cap->bus_info, sizeof(cap->bus_info));

	cap->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;

	return 0;
}

static int xtion_vidioc_g_fmt(struct file *fp, void *priv, struct v4l2_format *f)
{
	struct xtion_endpoint *endp = video_drvdata(fp);

	f->fmt.pix = endp->pix_fmt;

	return 0;
}

static int xtion_vidioc_s_fmt(struct file *fp, void *priv, struct v4l2_format *f)
{
	return 0;
}

static int xtion_vidioc_try_fmt(struct file *fp, void *priv, struct v4l2_format *f)
{
	return xtion_vidioc_g_fmt(fp, priv, f);
}

static const struct v4l2_ioctl_ops xtion_ioctls = {
	.vidioc_querycap      = xtion_vidioc_querycap,
	.vidioc_g_fmt_vid_cap = xtion_vidioc_g_fmt,
	.vidioc_s_fmt_vid_cap = xtion_vidioc_s_fmt,
	.vidioc_try_fmt_vid_cap = xtion_vidioc_try_fmt,

	/* vb2 takes care of these */
	.vidioc_reqbufs       = vb2_ioctl_reqbufs,
	.vidioc_querybuf      = vb2_ioctl_querybuf,
	.vidioc_qbuf          = vb2_ioctl_qbuf,
	.vidioc_dqbuf         = vb2_ioctl_dqbuf,
	.vidioc_streamon      = vb2_ioctl_streamon,
	.vidioc_streamoff     = vb2_ioctl_streamoff,
};


/******************************************************************************/
/*
 * videobuf2 operations
 */

static int xtion_vb2_setup(struct vb2_queue *q, const struct v4l2_format *format,
                           unsigned int *nbuffers, unsigned int *nplanes,
                           unsigned int sizes[], void *alloc_ctxs[])
{
	struct xtion_endpoint *endp = vb2_get_drv_priv(q);

	*nbuffers = clamp_t(unsigned int, *nbuffers, 2, 32);

	/* We only provide packed color formats */
	*nplanes = 1;

	sizes[0] = endp->pix_fmt.sizeimage;

	return 0;
}

static void xtion_vb2_queue(struct vb2_buffer *vb)
{
	struct xtion_endpoint *endp = vb2_get_drv_priv(vb->vb2_queue);
	struct xtion *xtion = endp->xtion;
	struct xtion_buffer *buf = container_of(vb, struct xtion_buffer, vb);
	unsigned long flags;

	spin_lock_irqsave(&endp->buf_lock, flags);

	if(!xtion->dev) {
		vb2_buffer_done(&buf->vb, VB2_BUF_STATE_ERROR);
	} else {
		buf->pos = 0;

		list_add_tail(&buf->list, &endp->avail_bufs);
	}

	spin_unlock_irqrestore(&endp->buf_lock, flags);
}

static int xtion_vb2_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct xtion_endpoint *endp = vb2_get_drv_priv(q);

	return xtion_enable_streaming(endp);
}

static int xtion_vb2_stop_streaming(struct vb2_queue *q)
{
	struct xtion_endpoint *endp = vb2_get_drv_priv(q);

	return xtion_disable_streaming(endp);
}

static const struct vb2_ops xtion_vb2_ops = {
	.queue_setup      = xtion_vb2_setup,
	.buf_queue        = xtion_vb2_queue,
	.start_streaming  = xtion_vb2_start_streaming,
	.stop_streaming   = xtion_vb2_stop_streaming,
	.wait_prepare     = vb2_ops_wait_prepare,
	.wait_finish      = vb2_ops_wait_finish
};

static void xtion_video_release(struct video_device *dev)
{
}


int xtion_endpoint_init(struct xtion_endpoint* endp, struct xtion* xtion, const struct xtion_endpoint_config *config)
{
	struct v4l2_pix_format* pix_format;
	int ret;

	endp->xtion = xtion;
	endp->config = config;

	/* Default video mode */
	pix_format = &endp->pix_fmt;

	pix_format->width = 640;
	pix_format->height = 480;
	pix_format->field = V4L2_FIELD_NONE;
	pix_format->colorspace = V4L2_COLORSPACE_SRGB;
	pix_format->pixelformat = V4L2_PIX_FMT_YUYV;
	pix_format->bytesperline = pix_format->width * 2;
	pix_format->sizeimage = pix_format->height * pix_format->bytesperline;
	pix_format->priv = 0;

	strncpy(endp->video.name, xtion->serial_number, sizeof(endp->video.name));
	endp->video.v4l2_dev = &xtion->v4l2_dev;
	endp->video.fops = &xtion_v4l2_fops;
	endp->video.release = &xtion_video_release;
	endp->video.ioctl_ops = &xtion_ioctls;

	video_set_drvdata(&endp->video, endp);

	/* Setup videobuf2 */
	endp->vb2.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	endp->vb2.io_modes = VB2_READ | VB2_MMAP | VB2_USERPTR;
	endp->vb2.drv_priv = endp;
	endp->vb2.buf_struct_size = sizeof(struct xtion_buffer);
	endp->vb2.ops = &xtion_vb2_ops;
	endp->vb2.mem_ops = &vb2_vmalloc_memops;
	endp->vb2.timestamp_type = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;

	ret = vb2_queue_init(&endp->vb2);
	if(ret < 0)
		return ret;

	INIT_LIST_HEAD(&endp->avail_bufs);

	endp->video.queue = &endp->vb2;

	ret = video_register_device(&endp->video, VFL_TYPE_GRABBER, -1);
	if(ret != 0)
		goto error_release_queue;

	ret = xtion_usb_init(endp);
	if(ret != 0)
		goto error_unregister;

	return 0;

error_unregister:
	video_unregister_device(&endp->video);
error_release_queue:
	vb2_queue_release(&endp->vb2);
	return ret;
}

void xtion_endpoint_release(struct xtion_endpoint* endp)
{
	xtion_usb_release(endp);
	video_unregister_device(&endp->video);
	vb2_queue_release(&endp->vb2);
}
