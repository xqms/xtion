/*
 * One data endpoint for ASUS Xtion Pro Live
 */

#include "xtion-endpoint.h"

#include <media/videobuf2-vmalloc.h>
#include <media/v4l2-ioctl.h>
#include <linux/usb.h>
#include <linux/slab.h>

#include "xtion-control.h"

static const __u16 intervals[] = {
	30, 60
};

/******************************************************************************/
/*
 * USB handling
 */

static void xtion_usb_process(struct xtion_endpoint *endp, const __u8 *data, unsigned int size)
{
	unsigned int off = 0;
	const __u8 *rptr;
	unsigned int to_read;

	if(WARN_ON(!endp || !endp->config)) {
		return;
	}

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
			to_read = min_t(unsigned int, size - off, sizeof(struct XtionSensorReplyHeader) - endp->packet_off);
			memcpy(((__u8*)&endp->packet_header) + endp->packet_off, rptr, to_read);
			endp->packet_off += to_read;
			off += to_read;

			if(endp->packet_off == sizeof(struct XtionSensorReplyHeader)) {
				/* header complete */

				endp->packet_data_size = ((endp->packet_header.bufSize_high << 8) | endp->packet_header.bufSize_low) - sizeof(struct XtionSensorReplyHeader);
				endp->packet_off = 0;
				endp->packet_state = XTION_PS_DATA;

				if(endp->packet_header.type == endp->config->start_id) {
					/* start of a new frame */
					endp->packet_id = endp->packet_header.packetID;
					endp->packet_corrupt = 0;

					/* padding information is returned in the timestamp field */
					endp->packet_pad_start = endp->packet_header.timestamp >> 16;
					endp->packet_pad_end = endp->packet_header.timestamp & 0xFFFF;

					/* save timestamp */
					v4l2_get_timestamp(&endp->packet_system_timestamp);

					/* new frame id */
					endp->frame_id++;

					endp->config->handle_start(endp);
				}  else {
					/* continuation, check packet ID */
					if(endp->packet_header.packetID != ((endp->packet_id + 1) & 0xFFFF)) {
						dev_warn(&endp->xtion->dev->dev, "Missed packets: %d -> %d\n", endp->packet_id, endp->packet_header.packetID);
						endp->packet_corrupt = 1;
					}

					endp->packet_id = endp->packet_header.packetID;
				}
			}
			break;
		case XTION_PS_DATA:
			to_read = min(size - off, endp->packet_data_size - endp->packet_off);

			if(!endp->packet_corrupt && to_read != 0)
				endp->config->handle_data(endp, rptr, to_read);

			endp->packet_off += to_read;
			off += to_read;

			if(endp->packet_off == endp->packet_data_size) {
				if(endp->packet_header.type == endp->config->end_id && !endp->packet_corrupt) {
					endp->packet_timestamp = endp->packet_header.timestamp;
					endp->config->handle_end(endp);
				}

				endp->packet_state = XTION_PS_MAGIC1;
			}
			break;
		}
	}
}

static void xtion_usb_irq_isoc(struct urb *urb)
{
	struct xtion_endpoint *endp = urb->context;
	int rc;
	int i;
	int status;
	unsigned int size;
	__u8 *p;

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
	for(i = 0; i < urb->number_of_packets; ++i) {
		status = urb->iso_frame_desc[i].status;
		if(status < 0 && status != -EPROTO) {
			if(status == -EPROTO)
				continue;

			dev_err(&endp->xtion->dev->dev, "Unknown packet status: %d\n", status);
			continue;
		}

		p = urb->transfer_buffer + urb->iso_frame_desc[i].offset;
		size = urb->iso_frame_desc[i].actual_length;

		xtion_usb_process(endp, p, size);
	}

	/* Reset urb buffers */
	for (i = 0; i < urb->number_of_packets; i++) {
		urb->iso_frame_desc[i].status = 0;
		urb->iso_frame_desc[i].actual_length = 0;
	}

	urb->transfer_flags = 0;
	urb->start_frame = usb_get_current_frame_number(endp->xtion->dev) + 1024;

	rc = usb_submit_urb(urb, GFP_ATOMIC);
	if(rc != 0)
		dev_err(&endp->xtion->dev->dev, "URB re-submit failed with code %d\n", rc);
}

static void xtion_usb_irq_bulk(struct urb *urb)
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
	for(i = 0; i < XTION_NUM_URBS; ++i) {
		if(endp->transfer_buffers[i]) {
			kfree(endp->transfer_buffers[i]);
			endp->transfer_buffers[i] = 0;
		}

		if(endp->urbs[i]) {
			usb_free_urb(endp->urbs[i]);
			endp->urbs[i] = 0;
		}
	}
}

/*
 * Compute the maximum number of bytes per interval for an endpoint.
 */
static unsigned int endpoint_max_bpi(struct usb_device *dev, struct usb_host_endpoint *ep)
{
	u16 psize;

	switch (dev->speed) {
	case USB_SPEED_SUPER:
		return ep->ss_ep_comp.wBytesPerInterval;
	case USB_SPEED_HIGH:
		psize = usb_endpoint_maxp(&ep->desc);
		return (psize & 0x07ff) * (1 + ((psize >> 11) & 3));
	default:
		psize = usb_endpoint_maxp(&ep->desc);
		return psize & 0x07ff;
	}
}

static int xtion_usb_init(struct xtion_endpoint* endp)
{
	struct urb *urb;
	int pipe;
	struct usb_host_endpoint *usb_endp;
	unsigned int psize;
	int i, j;
	unsigned int num_isoc_packets;

	if(endp->xtion->flags & XTION_FLAG_ISOC) {
		pipe = usb_rcvisocpipe(endp->xtion->dev, endp->config->addr);
		num_isoc_packets = 32;
	}
	else {
		pipe = usb_rcvbulkpipe(endp->xtion->dev, endp->config->addr);
		num_isoc_packets = 0;
	}

	usb_endp = usb_pipe_endpoint(endp->xtion->dev, pipe);
	psize = endpoint_max_bpi(endp->xtion->dev, usb_endp);

	memset(endp->transfer_buffers, 0, sizeof(endp->transfer_buffers));
	memset(endp->urbs, 0, sizeof(endp->transfer_buffers));

	for(i = 0; i < XTION_NUM_URBS; ++i)
	{
		urb = usb_alloc_urb(num_isoc_packets, GFP_KERNEL);
		if(!urb)
			return -ENOMEM;

		endp->urbs[i] = urb;
		endp->transfer_buffers[i] = kmalloc(XTION_URB_SIZE, GFP_KERNEL);

		if(!endp->transfer_buffers[i])
			goto free_buffers;

		urb->dev = endp->xtion->dev;
		urb->pipe = pipe;
		urb->transfer_buffer = endp->transfer_buffers[i];
		urb->context = endp;
		urb->start_frame = 0;
		urb->number_of_packets = num_isoc_packets;

		if(endp->xtion->flags & XTION_FLAG_ISOC) {
			urb->transfer_flags = URB_ISO_ASAP;
			urb->complete = xtion_usb_irq_isoc;
			urb->interval = 1;
			urb->transfer_buffer_length = XTION_URB_SIZE;
		}
		else {
			urb->transfer_flags = 0;
			urb->complete = xtion_usb_irq_bulk;
			urb->interval = 0;
			urb->transfer_buffer_length = endp->config->bulk_urb_size;
		}

		for(j = 0; j < urb->number_of_packets; ++j) {
			urb->iso_frame_desc[j].offset = j * psize;
			urb->iso_frame_desc[j].length = psize;
		}
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
	int base;
	struct xtion *xtion = endp->xtion;

	if(!xtion->dev)
		return -ENODEV;

	if(mutex_lock_interruptible(&xtion->control_mutex) != 0)
		return -EAGAIN;

	base = endp->config->settings_base;

	xtion_set_param(xtion, XTION_P_FRAME_SYNC, 0);
	xtion_set_param(xtion, XTION_P_REGISTRATION, 0);

	xtion_set_param(xtion, endp->config->endpoint_register, XTION_VIDEO_STREAM_OFF);
	xtion_set_param(xtion, base + XTION_CHANNEL_P_FORMAT, endp->config->image_format); // Uncompressed YUV422
	xtion_set_param(xtion, base + XTION_CHANNEL_P_RESOLUTION, endp->config->lookup_size(endp, endp->pix_fmt.width, endp->pix_fmt.height)); // VGA
	xtion_set_param(xtion, base + XTION_CHANNEL_P_FPS, endp->fps);
	xtion_set_param(xtion, endp->config->endpoint_register, endp->config->endpoint_mode);

	xtion_set_param(xtion, XTION_P_FRAME_SYNC, 1);
	xtion_set_param(xtion, XTION_P_REGISTRATION, 1);

	mutex_unlock(&xtion->control_mutex);

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

static void xtion_kill_urbs(struct xtion_endpoint *endp)
{
	int i;
	unsigned long flags;
	struct xtion_buffer *buf;

	/* Kill all pending URBs */
	for(i = 0; i < XTION_NUM_URBS; ++i) {
		if(endp->urbs[i]) {
			usb_kill_urb(endp->urbs[i]);
		}
	}

	/* Release all active buffers */
	spin_lock_irqsave(&endp->buf_lock, flags);
	while (!list_empty(&endp->avail_bufs)) {
			buf = list_first_entry(&endp->avail_bufs,
					struct xtion_buffer, list);
			list_del(&buf->list);
			vb2_buffer_done(&buf->vb, VB2_BUF_STATE_ERROR);
	}
	/* It's important to clear current buffer */
	endp->active_buffer = 0;
	spin_unlock_irqrestore(&endp->buf_lock, flags);
}

static int xtion_disable_streaming(struct xtion_endpoint *endp)
{
	/* Kill all submitted urbs */
	xtion_kill_urbs(endp);

	/* And disable streaming in the hardware */
// 	if(mutex_lock_interruptible(&endp->xtion->control_mutex) != 0) {
// 		return -EAGAIN;
// 	}
//
// 	xtion_set_param(endp->xtion, XTION_P_FRAME_SYNC, 0);
// 	xtion_set_param(endp->xtion, endp->config->endpoint_register, XTION_VIDEO_STREAM_OFF);

// 	mutex_unlock(&endp->xtion->control_mutex);

	return 0;
}

/******************************************************************************/
/*
 * v4l2 operations
 */

static struct v4l2_file_operations xtion_v4l2_fops = {
	.owner            = THIS_MODULE,
	.open             = v4l2_fh_open,
	.release          = vb2_fop_release,
	.mmap             = vb2_fop_mmap,
	.unlocked_ioctl   = video_ioctl2,
	.read             = vb2_fop_read,
	.poll             = vb2_fop_poll
};

/******************************************************************************/
/*
 * v4l2 ioctls
 */

static int xtion_vidioc_querycap(struct file *fp, void *priv, struct v4l2_capability *cap)
{
	struct xtion_endpoint *endp = video_drvdata(fp);

	strcpy(cap->driver, "xtion");
	strlcpy(cap->card, endp->video.name, sizeof(cap->card));

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

static int xtion_vidioc_try_fmt(struct file *fp, void *priv, struct v4l2_format *f)
{
	int framesize_code;
	struct xtion_endpoint *endp = video_drvdata(fp);

	struct v4l2_pix_format* pix = &f->fmt.pix;

	if(f->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	pix->pixelformat = endp->pix_fmt.pixelformat;

	framesize_code = endp->config->lookup_size(endp, pix->width, pix->height);
	if(framesize_code < 0) {
		pix->width = endp->pix_fmt.width;
		pix->height = endp->pix_fmt.height;
	}

	pix->field = V4L2_FIELD_NONE;

	pix->bytesperline = endp->config->pixel_size * pix->width;
	pix->sizeimage = pix->bytesperline * pix->height;

	return 0;
}

static int xtion_vidioc_s_fmt(struct file *fp, void *priv, struct v4l2_format *f)
{
	struct xtion_endpoint *endp = video_drvdata(fp);
	int rc;

	rc = xtion_vidioc_try_fmt(fp, priv, f);
	if(rc != 0)
		return rc;

	rc = mutex_lock_interruptible(&endp->vb2_lock);
	if(rc != 0)
		return -EAGAIN;

	if(vb2_is_busy(&endp->vb2)) {
		mutex_unlock(&endp->vb2_lock);
		return -EBUSY;
	}

	spin_lock(&endp->buf_lock);

	endp->pix_fmt = f->fmt.pix;

	spin_unlock(&endp->buf_lock);

	mutex_unlock(&endp->vb2_lock);
	return 0;
}

static int xtion_vidioc_enum_fmt(struct file *fp, void *priv, struct v4l2_fmtdesc *f)
{
	struct xtion_endpoint *endp = video_drvdata(fp);

	if(f->index != 0)
		return -EINVAL;

	strcpy(f->description, "YUV");
	f->pixelformat = endp->config->pix_fmt;

	return 0;
}

static int xtion_vidioc_enum_framesizes(struct file *fp, void *priv, struct v4l2_frmsizeenum *frms)
{
	struct xtion_endpoint *endp = video_drvdata(fp);

	if(frms->pixel_format != endp->config->pix_fmt)
		return -EINVAL;

	return endp->config->enumerate_sizes(endp, frms);
}

static int xtion_vidioc_enum_intervals(struct file *fp, void *priv, struct v4l2_frmivalenum *ival)
{
	if(ival->index >= ARRAY_SIZE(intervals))
		return -EINVAL;

	ival->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	ival->discrete.numerator = 1;
	ival->discrete.denominator = intervals[ival->index];

	return 0;
}

static int xtion_vidioc_g_parm(struct file *fp, void *priv, struct v4l2_streamparm *parm)
{
	struct xtion_endpoint *endp = video_drvdata(fp);

	parm->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	parm->parm.capture.capability = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_TIMEPERFRAME;
	parm->parm.capture.timeperframe.numerator = 1;
	parm->parm.capture.timeperframe.denominator = endp->fps;
	parm->parm.capture.extendedmode = 0;
	parm->parm.capture.readbuffers = 0;
	parm->parm.capture.capturemode = 0;

	return 0;
}

static int xtion_vidioc_s_parm(struct file *fp, void *priv, struct v4l2_streamparm *parm)
{
	struct xtion_endpoint *endp = video_drvdata(fp);
	int rc = 0;
	int closest = 0;
	int closest_diff = 1000;
	int i;

	if(parm->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	rc = mutex_lock_interruptible(&endp->vb2_lock);
	if(rc != 0)
		return -EAGAIN;

	if(vb2_is_busy(&endp->vb2)) {
		rc = -EBUSY;
		goto done;
	}

	endp->fps = parm->parm.capture.timeperframe.denominator;

	for(i = 0; i < ARRAY_SIZE(intervals); ++i) {
		if(abs((int)intervals[i] - endp->fps) < closest_diff) {
			closest_diff = abs((int)intervals[i] - endp->fps);
			closest = intervals[i];
		}
	}

	endp->fps = closest;
	parm->parm.capture.timeperframe.denominator = closest;
// 	xtion_set_param(endp->xtion, XTION_P_IMAGE_FPS, endp->fps);

done:
	mutex_unlock(&endp->vb2_lock);
	return rc;
}

static const struct v4l2_ioctl_ops xtion_ioctls = {
	.vidioc_querycap            = xtion_vidioc_querycap,
	.vidioc_g_fmt_vid_cap       = xtion_vidioc_g_fmt,
	.vidioc_s_fmt_vid_cap       = xtion_vidioc_s_fmt,
	.vidioc_try_fmt_vid_cap     = xtion_vidioc_try_fmt,
	.vidioc_enum_fmt_vid_cap    = xtion_vidioc_enum_fmt,
	.vidioc_enum_framesizes     = xtion_vidioc_enum_framesizes,
	.vidioc_enum_frameintervals = xtion_vidioc_enum_intervals,
	.vidioc_g_parm              = xtion_vidioc_g_parm,
	.vidioc_s_parm              = xtion_vidioc_s_parm,

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

static int xtion_vb2_prepare(struct vb2_buffer *vb)
{
	struct xtion_endpoint *endp = vb2_get_drv_priv(vb->vb2_queue);

	/* If we are already disconnected, do not allow queueing a new buffer */
	if(!endp->xtion->dev)
		return -ENODEV;

	return 0;
}

static int xtion_vb2_finish(struct vb2_buffer *vb)
{
	struct xtion_endpoint *endp = vb2_get_drv_priv(vb->vb2_queue);
	struct xtion_buffer *buf = container_of(vb, struct xtion_buffer, vb);

	if(!endp->config->uncompress)
		return 0;

	return endp->config->uncompress(endp, buf);
}

static void xtion_vb2_queue(struct vb2_buffer *vb)
{
	struct xtion_endpoint *endp = vb2_get_drv_priv(vb->vb2_queue);
	struct xtion *xtion = endp->xtion;
	struct xtion_buffer *buf = container_of(vb, struct xtion_buffer, vb);
	unsigned long flags;

	if(!xtion->dev) {
		vb2_buffer_done(&buf->vb, VB2_BUF_STATE_ERROR);
		return;
	}

	buf->pos = 0;

	spin_lock_irqsave(&endp->buf_lock, flags);
	list_add_tail(&buf->list, &endp->avail_bufs);
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
	.buf_prepare      = xtion_vb2_prepare,
	.buf_finish       = xtion_vb2_finish,
	.start_streaming  = xtion_vb2_start_streaming,
	.stop_streaming   = xtion_vb2_stop_streaming,
	.wait_prepare     = vb2_ops_wait_prepare,
	.wait_finish      = vb2_ops_wait_finish
};

/******************************************************************************/
/*
 * sysfs attributes
 */

ssize_t show_endpoint(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct video_device *vdev = container_of(dev, struct video_device, dev);
	struct xtion_endpoint *endp = container_of(vdev, struct xtion_endpoint, video);

	return snprintf(buf, PAGE_SIZE, "%s\n", endp->config->name)+1;
}

static DEVICE_ATTR(xtion_endpoint, S_IRUGO, show_endpoint, 0);


int xtion_endpoint_init(struct xtion_endpoint* endp, struct xtion* xtion, const struct xtion_endpoint_config *config)
{
	struct v4l2_pix_format* pix_format;
	int ret;

	endp->xtion = xtion;
	endp->config = config;
	endp->frame_id = 0;

	/* Default video mode */
	pix_format = &endp->pix_fmt;

	pix_format->width = 640;
	pix_format->height = 480;
	pix_format->field = V4L2_FIELD_NONE;
	pix_format->colorspace = V4L2_COLORSPACE_SRGB;
	pix_format->pixelformat = endp->config->pix_fmt;
	pix_format->bytesperline = pix_format->width * 2;
	pix_format->sizeimage = pix_format->height * pix_format->bytesperline;
	pix_format->priv = 0;

	/* Setup videobuf2 */
	mutex_init(&endp->vb2_lock);

	endp->vb2.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	endp->vb2.io_modes = VB2_READ | VB2_MMAP | VB2_USERPTR;
	endp->vb2.drv_priv = endp;
	endp->vb2.buf_struct_size = config->buffer_size;
	endp->vb2.ops = &xtion_vb2_ops;
	endp->vb2.mem_ops = &vb2_vmalloc_memops;
	endp->vb2.timestamp_type = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	endp->vb2.lock = &endp->vb2_lock;

	ret = vb2_queue_init(&endp->vb2);
	if(ret < 0)
		return ret;

	/* Setup video_device */
	snprintf(
		endp->video.name, sizeof(endp->video.name),
		"Xtion %s: %s",
		xtion->serial_number, endp->config->name
	);
	endp->video.v4l2_dev = &xtion->v4l2_dev;
	endp->video.lock = &xtion->control_mutex;
	endp->video.fops = &xtion_v4l2_fops;
	endp->video.release = &video_device_release_empty;
	endp->video.ioctl_ops = &xtion_ioctls;

	video_set_drvdata(&endp->video, endp);

	INIT_LIST_HEAD(&endp->avail_bufs);
	spin_lock_init(&endp->buf_lock);

	endp->video.queue = &endp->vb2;

	ret = video_register_device(&endp->video, VFL_TYPE_GRABBER, -1);
	if(ret != 0)
		goto error_release_queue;

	device_create_file(&endp->video.dev, &dev_attr_xtion_endpoint);

	ret = xtion_usb_init(endp);
	if(ret != 0)
		goto error_unregister;

	endp->packet_state = XTION_PS_MAGIC1;
	endp->packet_off = 0;
	endp->fps = 30;

	return 0;

error_unregister:
	video_unregister_device(&endp->video);
error_release_queue:
	device_remove_file(&endp->video.dev, &dev_attr_xtion_endpoint);
	vb2_queue_release(&endp->vb2);
	return ret;
}

void xtion_endpoint_release(struct xtion_endpoint* endp)
{
	xtion_usb_release(endp);
	device_remove_file(&endp->video.dev, &dev_attr_xtion_endpoint);
	video_unregister_device(&endp->video);
	vb2_queue_release(&endp->vb2);
}

void xtion_endpoint_disconnect(struct xtion_endpoint* endp)
{
	xtion_kill_urbs(endp);
}

/******************************************************************************/
/*
 * Endpoint implementation API
 */

struct xtion_buffer* xtion_endpoint_get_next_buf(struct xtion_endpoint* endp)
{
	unsigned long flags = 0;
	struct xtion_buffer *buf = NULL;

	spin_lock_irqsave(&endp->buf_lock, flags);
	if(list_empty(&endp->avail_bufs))
		goto leave;

	buf = list_entry(endp->avail_bufs.next, struct xtion_buffer, list);
	list_del(&buf->list);

leave:
	spin_unlock_irqrestore(&endp->buf_lock, flags);
	return buf;
}
