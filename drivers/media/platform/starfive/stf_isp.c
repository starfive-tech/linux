// SPDX-License-Identifier: GPL-2.0
/*
 * stf_isp.c
 *
 * StarFive Camera Subsystem - ISP Module
 *
 * Copyright (C) 2021-2023 StarFive Technology Co., Ltd.
 */
#include <linux/firmware.h>
#include <media/v4l2-event.h>

#include "stf_camss.h"

#define SINK_FORMATS_INDEX    0
#define UO_FORMATS_INDEX      1

static int isp_set_selection(struct v4l2_subdev *sd,
			     struct v4l2_subdev_state *state,
			     struct v4l2_subdev_selection *sel);

static const struct isp_format isp_formats_sink[] = {
	{ MEDIA_BUS_FMT_SRGGB10_1X10, 10 },
	{ MEDIA_BUS_FMT_SGRBG10_1X10, 10 },
	{ MEDIA_BUS_FMT_SGBRG10_1X10, 10 },
	{ MEDIA_BUS_FMT_SBGGR10_1X10, 10 },
};

static const struct isp_format isp_formats_uo[] = {
	{ MEDIA_BUS_FMT_Y12_1X12, 8 },
};

static const struct isp_format_table isp_formats_st7110[] = {
	{ isp_formats_sink, ARRAY_SIZE(isp_formats_sink) },
	{ isp_formats_uo, ARRAY_SIZE(isp_formats_uo) },
};

int stf_isp_subdev_init(struct stfcamss *stfcamss)
{
	struct stf_isp_dev *isp_dev = &stfcamss->isp_dev;

	isp_dev->sdev_type = STF_SUBDEV_TYPE_ISP;
	isp_dev->stfcamss = stfcamss;
	isp_dev->formats = isp_formats_st7110;
	isp_dev->nformats = ARRAY_SIZE(isp_formats_st7110);

	mutex_init(&isp_dev->stream_lock);
	mutex_init(&isp_dev->power_lock);
	return 0;
}

static int isp_set_power(struct v4l2_subdev *sd, int on)
{
	struct stf_isp_dev *isp_dev = v4l2_get_subdevdata(sd);

	mutex_lock(&isp_dev->power_lock);
	if (on) {
		if (isp_dev->power_count == 0)
			dev_dbg(isp_dev->stfcamss->dev, "turn on isp\n");
		isp_dev->power_count++;
	} else {
		if (isp_dev->power_count == 0)
			goto exit;
		isp_dev->power_count--;
	}
exit:
	mutex_unlock(&isp_dev->power_lock);

	return 0;
}

static struct v4l2_mbus_framefmt *
__isp_get_format(struct stf_isp_dev *isp_dev,
		 struct v4l2_subdev_state *state,
		 unsigned int pad,
		 enum v4l2_subdev_format_whence which)
{
	if (which == V4L2_SUBDEV_FORMAT_TRY)
		return v4l2_subdev_get_try_format(&isp_dev->subdev, state, pad);

	return &isp_dev->fmt[pad];
}

static int isp_set_stream(struct v4l2_subdev *sd, int enable)
{
	struct stf_isp_dev *isp_dev = v4l2_get_subdevdata(sd);
	int ret = 0;
	struct v4l2_mbus_framefmt *fmt;
	struct v4l2_event src_ch = { 0 };

	fmt = __isp_get_format(isp_dev, NULL, STF_ISP_PAD_SINK,
			       V4L2_SUBDEV_FORMAT_ACTIVE);
	mutex_lock(&isp_dev->stream_lock);
	if (enable) {
		if (isp_dev->stream_count == 0) {
			stf_isp_clk_enable(isp_dev);
			stf_isp_reset(isp_dev);
			stf_isp_init_cfg(isp_dev);
			stf_isp_settings(isp_dev, isp_dev->rect, fmt->code);
			stf_isp_stream_set(isp_dev);
		}
		isp_dev->stream_count++;
	} else {
		if (isp_dev->stream_count == 0)
			goto exit;

		if (isp_dev->stream_count == 1)
			stf_isp_clk_disable(isp_dev);

		isp_dev->stream_count--;
	}
	src_ch.type = V4L2_EVENT_SOURCE_CHANGE,
	src_ch.u.src_change.changes = isp_dev->stream_count,

	v4l2_subdev_notify_event(sd, &src_ch);
exit:
	mutex_unlock(&isp_dev->stream_lock);

	return ret;
}

static void isp_try_format(struct stf_isp_dev *isp_dev,
			   struct v4l2_subdev_state *state,
			   unsigned int pad,
			   struct v4l2_mbus_framefmt *fmt,
			   enum v4l2_subdev_format_whence which)
{
	const struct isp_format_table *formats;
	struct stf_isp_crop *rect;
	int i;

	switch (pad) {
	case STF_ISP_PAD_SINK:
		/* Set format on sink pad */
		formats = &isp_dev->formats[SINK_FORMATS_INDEX];
		rect = &isp_dev->rect[SINK_FORMATS_INDEX];
		fmt->width = clamp_t(u32,
				     fmt->width, STFCAMSS_FRAME_MIN_WIDTH,
				     STFCAMSS_FRAME_MAX_WIDTH);
		fmt->height = clamp_t(u32,
				      fmt->height, STFCAMSS_FRAME_MIN_HEIGHT,
				      STFCAMSS_FRAME_MAX_HEIGHT);
		fmt->height &= ~0x1;
		fmt->field = V4L2_FIELD_NONE;
		fmt->colorspace = V4L2_COLORSPACE_SRGB;
		fmt->flags = 0;
		break;

	case STF_ISP_PAD_SRC:
		formats = &isp_dev->formats[UO_FORMATS_INDEX];
		rect = &isp_dev->rect[UO_FORMATS_INDEX];
		*fmt = *__isp_get_format(isp_dev, state,
					 STF_ISP_PAD_SINK, which);
		break;
	}

	for (i = 0; i < formats->nfmts; i++) {
		if (fmt->code == formats->fmts[i].code)
			break;
	}

	if (i >= formats->nfmts) {
		fmt->code = formats->fmts[0].code;
		rect->bpp = formats->fmts[0].bpp;
	} else {
		rect->bpp = formats->fmts[i].bpp;
	}
}

static int isp_enum_mbus_code(struct v4l2_subdev *sd,
			      struct v4l2_subdev_state *state,
			      struct v4l2_subdev_mbus_code_enum *code)
{
	struct stf_isp_dev *isp_dev = v4l2_get_subdevdata(sd);
	const struct isp_format_table *formats;

	if (code->index >= isp_dev->nformats)
		return -EINVAL;
	if (code->pad == STF_ISP_PAD_SINK) {
		formats = &isp_dev->formats[SINK_FORMATS_INDEX];
		code->code = formats->fmts[code->index].code;
	} else {
		struct v4l2_mbus_framefmt *sink_fmt;

		sink_fmt = __isp_get_format(isp_dev, state, STF_ISP_PAD_SINK,
					    code->which);

		code->code = sink_fmt->code;
		if (!code->code)
			return -EINVAL;
	}
	code->flags = 0;

	return 0;
}

static int isp_enum_frame_size(struct v4l2_subdev *sd,
			       struct v4l2_subdev_state *state,
			       struct v4l2_subdev_frame_size_enum *fse)
{
	struct stf_isp_dev *isp_dev = v4l2_get_subdevdata(sd);
	struct v4l2_mbus_framefmt format;

	if (fse->index != 0)
		return -EINVAL;

	format.code = fse->code;
	format.width = 1;
	format.height = 1;
	isp_try_format(isp_dev, state, fse->pad, &format, fse->which);
	fse->min_width = format.width;
	fse->min_height = format.height;

	if (format.code != fse->code)
		return -EINVAL;

	format.code = fse->code;
	format.width = -1;
	format.height = -1;
	isp_try_format(isp_dev, state, fse->pad, &format, fse->which);
	fse->max_width = format.width;
	fse->max_height = format.height;

	return 0;
}

static int isp_get_format(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *state,
			  struct v4l2_subdev_format *fmt)
{
	struct stf_isp_dev *isp_dev = v4l2_get_subdevdata(sd);
	struct v4l2_mbus_framefmt *format;

	format = __isp_get_format(isp_dev, state, fmt->pad, fmt->which);
	if (!format)
		return -EINVAL;

	fmt->format = *format;

	return 0;
}

static int isp_set_format(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *state,
			  struct v4l2_subdev_format *fmt)
{
	struct stf_isp_dev *isp_dev = v4l2_get_subdevdata(sd);
	struct v4l2_mbus_framefmt *format;

	format = __isp_get_format(isp_dev, state, fmt->pad, fmt->which);
	if (!format)
		return -EINVAL;

	mutex_lock(&isp_dev->stream_lock);

	isp_try_format(isp_dev, state, fmt->pad, &fmt->format, fmt->which);
	*format = fmt->format;

	mutex_unlock(&isp_dev->stream_lock);

	/* Propagate to in crop */
	if (fmt->pad == STF_ISP_PAD_SINK) {
		struct v4l2_subdev_selection sel = { 0 };
		int ret;

		/* Reset sink pad compose selection */
		sel.which = fmt->which;
		sel.pad = STF_ISP_PAD_SINK;
		sel.target = V4L2_SEL_TGT_CROP;
		sel.r.width = fmt->format.width;
		sel.r.height = fmt->format.height;
		ret = isp_set_selection(sd, state, &sel);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static struct v4l2_rect *
__isp_get_crop(struct stf_isp_dev *isp_dev,
	       struct v4l2_subdev_state *state,
	       unsigned int pad,
	       enum v4l2_subdev_format_whence which)
{
	if (which == V4L2_SUBDEV_FORMAT_TRY)
		return v4l2_subdev_get_try_crop(&isp_dev->subdev, state,
						STF_ISP_PAD_SINK);

	return &isp_dev->rect[pad].rect;
}

static void isp_try_crop(struct stf_isp_dev *isp_dev,
			 struct v4l2_subdev_state *state,
			 struct v4l2_rect *rect,
			 enum v4l2_subdev_format_whence which)
{
	struct v4l2_mbus_framefmt *fmt;

	fmt = __isp_get_format(isp_dev, state, STF_ISP_PAD_SINK, which);

	if (rect->width > fmt->width)
		rect->width = fmt->width;

	if (rect->width + rect->left > fmt->width)
		rect->left = fmt->width - rect->width;

	if (rect->height > fmt->height)
		rect->height = fmt->height;

	if (rect->height + rect->top > fmt->height)
		rect->top = fmt->height - rect->height;

	if (rect->width < STFCAMSS_FRAME_MIN_WIDTH) {
		rect->left = 0;
		rect->width = STFCAMSS_FRAME_MAX_WIDTH;
	}

	if (rect->height < STFCAMSS_FRAME_MIN_HEIGHT) {
		rect->top = 0;
		rect->height = STFCAMSS_FRAME_MAX_HEIGHT;
	}
	rect->height &= ~0x1;
}

static int isp_get_selection(struct v4l2_subdev *sd,
			     struct v4l2_subdev_state *state,
			     struct v4l2_subdev_selection *sel)
{
	struct stf_isp_dev *isp_dev = v4l2_get_subdevdata(sd);
	struct v4l2_subdev_format fmt = { 0 };
	struct v4l2_rect *rect;
	int ret;

	switch (sel->target) {
	case V4L2_SEL_TGT_CROP_BOUNDS:
		if (sel->pad == STF_ISP_PAD_SINK) {
			fmt.pad = sel->pad;
			fmt.which = sel->which;
			ret = isp_get_format(sd, state, &fmt);
			if (ret < 0)
				return ret;

			sel->r.left = 0;
			sel->r.top = 0;
			sel->r.width = fmt.format.width;
			sel->r.height = fmt.format.height;
		} else if (sel->pad == STF_ISP_PAD_SRC) {
			rect = __isp_get_crop(isp_dev, state,
					      sel->pad, sel->which);
			sel->r = *rect;
		}
		break;

	case V4L2_SEL_TGT_CROP:
		rect = __isp_get_crop(isp_dev, state, sel->pad, sel->which);
		if (!rect)
			return -EINVAL;

		sel->r = *rect;
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int isp_set_selection(struct v4l2_subdev *sd,
			     struct v4l2_subdev_state *state,
			     struct v4l2_subdev_selection *sel)
{
	struct stf_isp_dev *isp_dev = v4l2_get_subdevdata(sd);
	struct v4l2_rect *rect;
	int ret = 0;

	if (sel->target == V4L2_SEL_TGT_CROP &&
	    sel->pad == STF_ISP_PAD_SINK) {
		struct v4l2_subdev_selection crop = { 0 };

		rect = __isp_get_crop(isp_dev, state, sel->pad, sel->which);
		if (!rect)
			return -EINVAL;

		mutex_lock(&isp_dev->stream_lock);
		isp_try_crop(isp_dev, state, &sel->r, sel->which);
		*rect = sel->r;
		mutex_unlock(&isp_dev->stream_lock);

		/* Reset source crop selection */
		crop.which = sel->which;
		crop.pad = STF_ISP_PAD_SRC;
		crop.target = V4L2_SEL_TGT_CROP;
		crop.r = *rect;
		ret = isp_set_selection(sd, state, &crop);
	} else if (sel->target == V4L2_SEL_TGT_CROP &&
		   sel->pad == STF_ISP_PAD_SRC) {
		struct v4l2_subdev_format fmt = { 0 };

		rect = __isp_get_crop(isp_dev, state, sel->pad, sel->which);
		if (!rect)
			return -EINVAL;

		mutex_lock(&isp_dev->stream_lock);
		isp_try_crop(isp_dev, state, &sel->r, sel->which);
		*rect = sel->r;
		mutex_unlock(&isp_dev->stream_lock);

		/* Reset source pad format width and height */
		fmt.which = sel->which;
		fmt.pad = STF_ISP_PAD_SRC;
		fmt.format.width = rect->width;
		fmt.format.height = rect->height;
		ret = isp_set_format(sd, state, &fmt);
		if (ret < 0)
			return ret;
	}

	dev_dbg(isp_dev->stfcamss->dev, "pad: %d sel(%d,%d)/%dx%d\n",
		sel->pad, sel->r.left, sel->r.top, sel->r.width, sel->r.height);

	return 0;
}

static int isp_init_formats(struct v4l2_subdev *sd,
			    struct v4l2_subdev_fh *fh)
{
	struct v4l2_subdev_format format = {
		.pad = STF_ISP_PAD_SINK,
		.which =
			fh ? V4L2_SUBDEV_FORMAT_TRY : V4L2_SUBDEV_FORMAT_ACTIVE,
		.format = {
			.code = MEDIA_BUS_FMT_RGB565_2X8_LE,
			.width = 1920,
			.height = 1080
		}
	};

	return isp_set_format(sd, fh ? fh->state : NULL, &format);
}

static int isp_link_setup(struct media_entity *entity,
			  const struct media_pad *local,
			  const struct media_pad *remote, u32 flags)
{
	if (flags & MEDIA_LNK_FL_ENABLED)
		if (media_pad_remote_pad_first(local))
			return -EBUSY;
	return 0;
}

static int stf_isp_subscribe_event(struct v4l2_subdev *sd,
				   struct v4l2_fh *fh,
				   struct v4l2_event_subscription *sub)
{
	switch (sub->type) {
	case V4L2_EVENT_SOURCE_CHANGE:
		return v4l2_src_change_event_subdev_subscribe(sd, fh, sub);
	default:
		return -EINVAL;
	}
}

static const struct v4l2_subdev_core_ops isp_core_ops = {
	.s_power = isp_set_power,
	.subscribe_event = stf_isp_subscribe_event,
	.unsubscribe_event = v4l2_event_subdev_unsubscribe,
};

static const struct v4l2_subdev_video_ops isp_video_ops = {
	.s_stream = isp_set_stream,
};

static const struct v4l2_subdev_pad_ops isp_pad_ops = {
	.enum_mbus_code = isp_enum_mbus_code,
	.enum_frame_size = isp_enum_frame_size,
	.get_fmt = isp_get_format,
	.set_fmt = isp_set_format,
	.get_selection = isp_get_selection,
	.set_selection = isp_set_selection,
};

static const struct v4l2_subdev_ops isp_v4l2_ops = {
	.core = &isp_core_ops,
	.video = &isp_video_ops,
	.pad = &isp_pad_ops,
};

static const struct v4l2_subdev_internal_ops isp_v4l2_internal_ops = {
	.open = isp_init_formats,
};

static const struct media_entity_operations isp_media_ops = {
	.link_setup = isp_link_setup,
	.link_validate = v4l2_subdev_link_validate,
};

int stf_isp_register(struct stf_isp_dev *isp_dev, struct v4l2_device *v4l2_dev)
{
	struct v4l2_subdev *sd = &isp_dev->subdev;
	struct media_pad *pads = isp_dev->pads;
	int ret;

	v4l2_subdev_init(sd, &isp_v4l2_ops);
	sd->internal_ops = &isp_v4l2_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS;
	snprintf(sd->name, ARRAY_SIZE(sd->name), "%s%d", STF_ISP_NAME, 0);
	v4l2_set_subdevdata(sd, isp_dev);

	ret = isp_init_formats(sd, NULL);
	if (ret < 0) {
		dev_err(isp_dev->stfcamss->dev, "Failed to init format: %d\n",
			ret);
		return ret;
	}

	pads[STF_ISP_PAD_SINK].flags = MEDIA_PAD_FL_SINK;
	pads[STF_ISP_PAD_SRC].flags = MEDIA_PAD_FL_SOURCE;

	sd->entity.function = MEDIA_ENT_F_PROC_VIDEO_PIXEL_FORMATTER;
	sd->entity.ops = &isp_media_ops;
	ret = media_entity_pads_init(&sd->entity, STF_ISP_PAD_MAX, pads);
	if (ret < 0) {
		dev_err(isp_dev->stfcamss->dev,
			"Failed to init media entity: %d\n", ret);
		return ret;
	}

	ret = v4l2_device_register_subdev(v4l2_dev, sd);
	if (ret < 0) {
		dev_err(isp_dev->stfcamss->dev,
			"Failed to register subdev: %d\n", ret);
		goto err_sreg;
	}

	return 0;

err_sreg:
	media_entity_cleanup(&sd->entity);
	return ret;
}

int stf_isp_unregister(struct stf_isp_dev *isp_dev)
{
	v4l2_device_unregister_subdev(&isp_dev->subdev);
	media_entity_cleanup(&isp_dev->subdev.entity);
	mutex_destroy(&isp_dev->stream_lock);
	mutex_destroy(&isp_dev->power_lock);
	return 0;
}
