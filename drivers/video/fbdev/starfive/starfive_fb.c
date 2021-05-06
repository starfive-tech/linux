/* driver/video/starfive/starfivefb.c
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License version 2 as
** published by the Free Software Foundation.
**
** Copyright (C) 2020 StarFive, Inc.
**
** PURPOSE:	This files contains the driver of LCD controller and VPP.
**
** CHANGE HISTORY:
**	Version		Date		Author		Description
**	0.1.0		2020-10-07	starfive		created
**
*/

#include <linux/errno.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/compiler.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/cpufreq.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/uaccess.h>
#include <linux/notifier.h>
#include <linux/mtd/mtd.h>
#include <linux/workqueue.h>
#include <linux/module.h>
#include <linux/leds.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/div64.h>
#include <asm/cacheflush.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_address.h>
#include <video/stf-vin.h>

#include "starfive_fb.h"
#include "starfive_lcdc.h"
#include "starfive_vpp.h"
#include "starfive_display_dev.h"
#include "starfive_mipi_tx.h"

static struct sf_fb_data *stf_dev = NULL;

static DEFINE_MUTEX(stf_mutex);

//#define SF_FB_DEBUG	1
#ifdef SF_FB_DEBUG
	#define FB_PRT(format, args...)  printk(KERN_DEBUG "[FB]: " format, ## args)
	#define FB_INFO(format, args...) printk(KERN_INFO "[FB]: " format, ## args)
	#define FB_ERR(format, args...)	printk(KERN_ERR "[FB]: " format, ## args)
#else
	#define FB_PRT(x...)  do{} while(0)
	#define FB_INFO(x...)  do{} while(0)
	#define FB_ERR(x...)  do{} while(0)
#endif

static const struct res_name mem_res_name[] = {
	{"lcdc"},
	{"dsitx"},
	{"vpp0"},
	{"vpp1"},
	{"vpp2"},
	{"clk"},
	{"rst"},
	{"sys"}
};

static u32 sf_fb_clkread32(struct sf_fb_data *sf_dev, u32 reg)
{
	return ioread32(sf_dev->base_clk + reg);
}

static void sf_fb_clkwrite32(struct sf_fb_data *sf_dev, u32 reg, u32 val)
{
	iowrite32(val, sf_dev->base_clk + reg);
}

static int sf_fb_lcdc_clk_cfg(struct sf_fb_data *sf_dev)
{
	u32 tmp_val = 0;
	int ret = 0;

	switch(sf_dev->display_info.xres) {
		case 640:
			dev_warn(sf_dev->dev, "640 do nothing! need to set clk\n");
			break;
		case 800:
			tmp_val = sf_fb_clkread32(sf_dev, CLK_LCDC_OCLK_CTRL);
			tmp_val &= ~(0x3F);
			tmp_val |= (54 & 0x3F);
			sf_fb_clkwrite32(sf_dev, CLK_LCDC_OCLK_CTRL, tmp_val);
			break;
		case 1280:
			dev_warn(sf_dev->dev, "1280 do nothing! need to set clk\n");
			break;
		case 1920:
			tmp_val = sf_fb_clkread32(sf_dev, CLK_LCDC_OCLK_CTRL);
			tmp_val &= ~(0x3F);
			tmp_val |= (24 & 0x3F);
			sf_fb_clkwrite32(sf_dev, CLK_LCDC_OCLK_CTRL, tmp_val);
			break;
		default:
			dev_err(sf_dev->dev, "Fail to allocate video RAM\n");
			ret = -EINVAL;
	}

	return ret;
}

#if defined(CONFIG_VIDEO_STARFIVE_VIN)
static int vin_frame_complete_notify(struct notifier_block *nb,
				      unsigned long val, void *v)
{
	struct vin_params *psy = v;
	struct sf_fb_data *sf_dev = stf_dev;
	unsigned int address;
	unsigned int u_addr, v_addr, size;
	unsigned int y_rgb_offset, u_offset, v_offset;

	address = (unsigned int)psy->paddr;

	if(NULL == sf_dev) {
		return NOTIFY_OK;
	}

	if(sf_dev->pp_conn_lcdc < 0) {
		//dev_warn(sf_dev->dev, "%s NO use PPx\n",__func__);
	} else {
		if(sf_dev->pp[sf_dev->pp_conn_lcdc].src.format >= COLOR_RGB888_ARGB) {
	        u_addr = 0;
	        v_addr = 0;
	        y_rgb_offset = 0;
	        u_offset = 0;
	        v_offset = 0;
		} else if (COLOR_YUV420_NV21 == sf_dev->pp[sf_dev->pp_conn_lcdc].src.format) {
			size = sf_dev->display_info.xres * sf_dev->display_info.yres;
			u_addr = address + size + 1;
			v_addr = address + size;
			y_rgb_offset = 0;
			u_offset = 0;
			v_offset = size;
		} else {
			dev_err(sf_dev->dev, "format %d not SET\n", sf_dev->pp[sf_dev->pp_conn_lcdc].src.format);
			return -EINVAL;
		}
		pp_srcAddr_next(sf_dev, sf_dev->pp_conn_lcdc, address, u_addr, v_addr);
		pp_srcOffset_cfg(sf_dev, sf_dev->pp_conn_lcdc, y_rgb_offset, u_offset, v_offset);
		//pp_run(sf_dev, sf_dev->pp_conn_lcdc, PP_RUN);
	}

	return NOTIFY_OK;
}
#endif

static int sf_get_mem_res(struct platform_device *pdev, struct sf_fb_data *sf_dev)
{
	struct device *dev = &pdev->dev;
	struct resource	*res;
	void __iomem *regs;
	char *name;
	int i;

	for (i = 0; i < sizeof(mem_res_name)/sizeof(struct res_name); i++) {
	    name = (char *)(& mem_res_name[i]);
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, name);
		regs = devm_ioremap_resource(dev, res);
		if (IS_ERR(regs))
			return PTR_ERR(regs);

		if(!strcmp(name, "lcdc")) {
			sf_dev->base_lcdc = regs;
		} else if (!strcmp(name, "dsitx")) {
			sf_dev->base_dsitx = regs;
		} else if (!strcmp(name, "vpp0")) {
			sf_dev->base_vpp0 = regs;
		} else if (!strcmp(name, "vpp1")) {
			sf_dev->base_vpp1 = regs;
		} else if (!strcmp(name, "vpp2")) {
			sf_dev->base_vpp2 = regs;
		} else if (!strcmp(name, "clk")) {
			sf_dev->base_clk = regs;
		} else if (!strcmp(name, "rst")) {
			sf_dev->base_rst = regs;
		} else if (!strcmp(name, "sys")) {
			sf_dev->base_syscfg = regs;
		} else {
			dev_err(&pdev->dev, "Could not match resource name\n");
		}
	}

	return 0;
}

static void sf_fb_get_var(struct fb_var_screeninfo *var, struct sf_fb_data *sf_dev)
{
	var->xres		= sf_dev->display_info.xres;
	var->yres		= sf_dev->display_info.yres;
	var->bits_per_pixel	= sf_dev->display_dev->bpp;
#if defined(CONFIG_FRAMEBUFFER_CONSOLE)
	if(24 == var->bits_per_pixel)
	{
		var->bits_per_pixel	= 16;//as vpp&lcdc miss support rgb888 ,config fb_console src format as rgb565 
	}
#endif
	var->pixclock		= 1000000 / (sf_dev->pixclock / 1000000);
	var->hsync_len		= sf_dev->display_info.hsync_len;
	var->vsync_len		= sf_dev->display_info.vsync_len;
	var->left_margin	= sf_dev->display_info.left_margin;
	var->right_margin	= sf_dev->display_info.right_margin;
	var->upper_margin	= sf_dev->display_info.upper_margin;
	var->lower_margin	= sf_dev->display_info.lower_margin;
	var->sync		= sf_dev->display_info.sync;
	var->vmode		= FB_VMODE_NONINTERLACED;
}

/*
 * sf_fb_set_par():
 *	Set the user defined part of the display for the specified console
 */
static int sf_fb_set_par(struct fb_info *info)
{
	struct sf_fb_data *sf_dev = container_of(info, struct sf_fb_data, fb);
	struct fb_var_screeninfo *var = &info->var;

	FB_PRT("%s,%d\n",__func__, __LINE__);

	if (var->bits_per_pixel == 16 ||
		var->bits_per_pixel == 18 ||
		var->bits_per_pixel == 24 ||
		var->bits_per_pixel == 32)
		sf_dev->fb.fix.visual = FB_VISUAL_TRUECOLOR;
	else if (!sf_dev->cmap_static)
		sf_dev->fb.fix.visual = FB_VISUAL_PSEUDOCOLOR;
	else {
		/*
		 * Some people have weird ideas about wanting static
		 * pseudocolor maps.  I suspect their user space
		 * applications are broken.
		 */
		sf_dev->fb.fix.visual = FB_VISUAL_STATIC_PSEUDOCOLOR;
	}

	sf_dev->fb.fix.line_length = var->xres_virtual * var->bits_per_pixel / 8;

	if (sf_dev->fb.var.bits_per_pixel == 16 ||
		sf_dev->fb.var.bits_per_pixel == 18 ||
		sf_dev->fb.var.bits_per_pixel == 24 ||
		sf_dev->fb.var.bits_per_pixel == 32)
		fb_dealloc_cmap(&sf_dev->fb.cmap);
	else
		fb_alloc_cmap(&sf_dev->fb.cmap,
			1 << sf_dev->fb.var.bits_per_pixel, 0);

	/*for fbcon, it need cmap*/
	switch(var->bits_per_pixel) {
		case 16:
			var->red.offset   = 11; var->red.length   = 5;
			var->green.offset = 5;	var->green.length = 6;
			var->blue.offset  = 0;	var->blue.length  = 5;
			var->transp.offset = var->transp.length = 0;
			break;
		case 18:
			var->red.offset   = 12; var->red.length   = 6;
			var->green.offset = 6;	var->green.length = 6;
			var->blue.offset  = 0;	var->blue.length  = 6;
			var->transp.offset = var->transp.length = 0;
			break;
		case 24:
			var->red.offset   = 16; var->red.length   = 8;
			var->green.offset = 8;	var->green.length = 8;
			var->blue.offset  = 0;	var->blue.length  = 8;
			var->transp.offset = var->transp.length = 0;
			break;
		case 32:
			var->red.offset   = 16; var->red.length   = 8;
			var->green.offset = 8;	var->green.length = 8;
			var->blue.offset  = 0;	var->blue.length  = 8;
			var->transp.offset = var->transp.length = 0;
			break;
		default:
			var->red.offset = var->green.offset = \
				var->blue.offset = var->transp.offset = 0;
			var->red.length   = 8;
			var->green.length = 8;
			var->blue.length  = 8;
			var->transp.length = 0;
	}

	if (!strcmp(sf_dev->dis_dev_name, "tda_998x_1080p")) {
		var->red.offset   = 0;  var->red.length   = 5;
		var->green.offset = 5;	var->green.length = 6;
		var->blue.offset  = 11;	var->blue.length  = 5;
		var->transp.offset = var->transp.length = 0;
	}

	return 0;
}

static int sf_fb_open(struct fb_info *info, int user)
{
	struct sf_fb_data *sf_dev = container_of(info, struct sf_fb_data, fb);

	FB_PRT("%s,%d\n",__func__, __LINE__);

	sf_fb_set_par(info);
	lcdc_run(sf_dev, sf_dev->winNum, LCDC_RUN);

	//sf_fb_init_layer(layer, &info->var);
	//if (layer->no == sf_fb_ids[0])
		//sf_fb_enable_layer(layer);
	return 0;
}

static int sf_fb_release(struct fb_info *info, int user)
{
	struct sf_fb_data *sf_dev = container_of(info, struct sf_fb_data, fb);

	FB_PRT("%s,%d\n",__func__, __LINE__);

	lcdc_run(sf_dev, sf_dev->winNum, LCDC_STOP);

	return 0;
}

static int sf_fb_ioctl(struct fb_info *info, unsigned cmd, unsigned long arg)
{
//	struct sf_fb_data *sf_dev = container_of(info, struct sf_fb_data, fb);

	FB_PRT("%s,%d\n",__func__, __LINE__);
	return 0;
}

/*
 *	sf_fb_check_var():
 *	  Get the video params out of 'var'. If a value doesn't fit, round it up,
 *	  if it's too big, return -EINVAL.
 *
 *	  Round up in the following order: bits_per_pixel, xres,
 *	  yres, xres_virtual, yres_virtual, xoffset, yoffset, grayscale,
 *	  bitfields, horizontal timing, vertical timing.
 */
static int sf_fb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	struct sf_fb_data *sf_dev = container_of(info, struct sf_fb_data, fb);

	FB_PRT("%s,%d\n",__func__, __LINE__);

	if (var->xres < MIN_XRES)
		var->xres = MIN_XRES;
	if (var->yres < MIN_YRES)
		var->yres = MIN_YRES;

	sf_fb_get_var(var, sf_dev);

	var->xres_virtual = var->xres;
	var->yres_virtual = var->yres * sf_dev->buf_num;
	/*
	 * Setup the RGB parameters for this display.
	 */
	switch(var->bits_per_pixel) {
		case 16:
			var->red.offset   = 11; var->red.length   = 5;
			var->green.offset = 5;	var->green.length = 6;
			var->blue.offset  = 0;	var->blue.length  = 5;
			var->transp.offset = var->transp.length = 0;
			break;
		case 18:
			var->red.offset   = 12; var->red.length   = 6;
			var->green.offset = 6;	var->green.length = 6;
			var->blue.offset  = 0;	var->blue.length  = 6;
			var->transp.offset = var->transp.length = 0;
			break;
		case 24:
			var->red.offset   = 16; var->red.length   = 8;
			var->green.offset = 8;	var->green.length = 8;
			var->blue.offset  = 0;	var->blue.length  = 8;
			var->transp.offset = var->transp.length = 0;
			break;
		case 32:
			var->red.offset   = 16; var->red.length   = 8;
			var->green.offset = 8;	var->green.length = 8;
			var->blue.offset  = 0;	var->blue.length  = 8;
			var->transp.offset = var->transp.length = 0;
			break;
		default:
			var->red.offset = var->green.offset = \
				var->blue.offset = var->transp.offset = 0;
			var->red.length   = 8;
			var->green.length = 8;
			var->blue.length  = 8;
			var->transp.length = 0;
	}

	if (!strcmp(sf_dev->dis_dev_name, "tda_998x_1080p")) {
		var->red.offset   = 0;  var->red.length   = 5;
		var->green.offset = 5;	var->green.length = 6;
		var->blue.offset  = 11;	var->blue.length  = 5;
		var->transp.offset = var->transp.length = 0;
	}

	return 0;
}

static inline u_int sf_chan_to_field(u_int chan, struct fb_bitfield *bf)
{
	chan &= 0xffff;
	chan >>= 16 - bf->length;
	return chan << bf->offset;
}

static int sf_fb_setcolreg(u_int regno, u_int red, u_int green,
			u_int blue, u_int trans, struct fb_info *info)
{
	struct sf_fb_data *sf_dev = container_of(info, struct sf_fb_data, fb);
	unsigned int val;
	int ret = 1;

	FB_PRT("%s,%d\n",__func__, __LINE__);
	/*
	 * If inverse mode was selected, invert all the colours
	 * rather than the register number.  The register number
	 * is what you poke into the framebuffer to produce the
	 * colour you requested.
	 */
	if (sf_dev->cmap_inverse) {
		red	= 0xffff - red;
		green	= 0xffff - green;
		blue	= 0xffff - blue;
	}

	/*
	 * If greyscale is true, then we convert the RGB value
	 * to greyscale no matter what visual we are using.
	 */
	if (sf_dev->fb.var.grayscale)
		red = green = blue = (19595 * red + 38470 * green +
					7471 * blue) >> 16;

	switch (sf_dev->fb.fix.visual) {
	case FB_VISUAL_TRUECOLOR:
		/*
		 * 16-bit True Colour.	We encode the RGB value
		 * according to the RGB bitfield information.
		 */
		if (regno < 16) {
			u32 *pal = sf_dev->fb.pseudo_palette;

			val = sf_chan_to_field(red, &sf_dev->fb.var.red);
			val |= sf_chan_to_field(green, &sf_dev->fb.var.green);
			val |= sf_chan_to_field(blue, &sf_dev->fb.var.blue);

			pal[regno] = val;
			ret = 0;
		}
	break;

	case FB_VISUAL_STATIC_PSEUDOCOLOR:
	case FB_VISUAL_PSEUDOCOLOR:
		/* haven't support this function. */
	break;
	}

	return 0;
}

/*
 * sf_fb_set_addr():
 *	Configures LCD Controller based on entries in var parameter.  Settings are
 *	only written to the controller if changes were made.
 */
static int sf_fb_set_addr(struct sf_fb_data *sf_dev, struct fb_var_screeninfo *var)
{
	unsigned int address;
	unsigned int offset;
	unsigned int u_addr, v_addr, size;
	unsigned int y_rgb_offset, u_offset, v_offset;
	int i = 0;

	offset = var->yoffset * sf_dev->fb.fix.line_length +
				var->xoffset * var->bits_per_pixel / 8;
	address = sf_dev->fb.fix.smem_start + offset;
	size = var->xres * var->yres;

	FB_PRT("%s,%d\n",__func__, __LINE__);

	if(sf_dev->pp_conn_lcdc < 0) {
		dev_warn(sf_dev->dev, "%s NO use PPx\n",__func__);
	} else {
			if(sf_dev->pp[sf_dev->pp_conn_lcdc].src.format >= COLOR_RGB888_ARGB) {
			u_addr = 0;
			v_addr = 0;
			y_rgb_offset = 0;
			u_offset = 0;
			v_offset = 0;
		} else if (COLOR_YUV420_NV21 == sf_dev->pp[sf_dev->pp_conn_lcdc].src.format) {
			u_addr = address + size + 1;
			v_addr = address + size;
			y_rgb_offset = 0;
			u_offset = 0;
			v_offset = size;
		} else if (COLOR_YUV420_NV12 == sf_dev->pp[sf_dev->pp_conn_lcdc].src.format) {
			u_addr = address + size ;
			v_addr = address + size + 1;
			y_rgb_offset = 0;
			u_offset = 0;
			v_offset = size;
		} else {
			dev_err(sf_dev->dev, "format %d not SET\n",
				sf_dev->pp[sf_dev->pp_conn_lcdc].src.format);
			return -EINVAL;
		}
		pp_srcAddr_next(sf_dev, sf_dev->pp_conn_lcdc, address, u_addr, v_addr);
		pp_srcOffset_cfg(sf_dev, sf_dev->pp_conn_lcdc, y_rgb_offset, u_offset, v_offset);
		pp_nxtAddr_load(sf_dev, sf_dev->pp_conn_lcdc, 0x1, (i & 0x1));
		pp_run(sf_dev, sf_dev->pp_conn_lcdc, PP_RUN);
	}

	return 0;
}


static int sf_fb_pan_display(struct fb_var_screeninfo *var,
				struct fb_info *info)
{
	struct sf_fb_data *sf_dev = container_of(info, struct sf_fb_data, fb);

	FB_PRT("%s,%d\n",__func__, __LINE__);

	sf_fb_set_addr(sf_dev, var);

	switch(sf_dev->display_dev->interface_info) {
		case STARFIVEFB_MIPI_IF:
		case STARFIVEFB_HDMI_IF:
			//lcdc_run(sf_dev,0x2, 0x1);
			break;
		case STARFIVEFB_RGB_IF:
			break;
		default:
			break;
	}

	return 0;
}

static int sf_fb_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
	unsigned long off = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long start;
	u32 len;

	FB_PRT("%s,%d\n",__func__, __LINE__);
	/* frame buffer memory */
	start = info->fix.smem_start;
	len = PAGE_ALIGN((start & ~PAGE_MASK) + info->fix.smem_len);

	if (off >= len) {
		/* memory mapped io */
		off -= len;
		if (info->var.accel_flags) {
			mutex_unlock(&info->mm_lock);
			return -EINVAL;
		}
		start = info->fix.mmio_start;
		len = PAGE_ALIGN((start & ~PAGE_MASK) + info->fix.mmio_len);
	}

	start &= PAGE_MASK;
	if ((vma->vm_end - vma->vm_start + off) > len) {
		return -EINVAL;
	}

	off += start;
	vma->vm_pgoff = off >> PAGE_SHIFT;
	/* This is an IO map - tell maydump to skip this VMA */
//	vma->vm_flags |= VM_IO | VM_RESERVED;
	vma->vm_flags |= VM_IO;

//	if (!(layer->parent->pdata->flags & FB_CACHED_BUFFER))
//		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	if (io_remap_pfn_range(vma, vma->vm_start, off >> PAGE_SHIFT,
			     vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
		return -EAGAIN;
	}

	return 0;
}

static int sf_fb_blank(int blank, struct fb_info *info)
{
	struct sf_fb_data *sf_dev = container_of(info, struct sf_fb_data, fb);
	int ret;

	FB_PRT("%s,%d\n",__func__, __LINE__);

	switch (blank) {
		case FB_BLANK_UNBLANK:
			lcdc_run(sf_dev,0x2, 0x1);
			break;
		case FB_BLANK_NORMAL:
		case FB_BLANK_VSYNC_SUSPEND:
		case FB_BLANK_HSYNC_SUSPEND:
		case FB_BLANK_POWERDOWN:
			lcdc_run(sf_dev,0x2, 0x0);
			break;
		default:
			ret = -EINVAL;
	}

	return ret;
}

static struct fb_ops sf_fb_ops = {
	.owner		= THIS_MODULE,
	.fb_open	= sf_fb_open,
	.fb_release	= sf_fb_release,
	.fb_ioctl	= sf_fb_ioctl,
	.fb_check_var	= sf_fb_check_var,
	.fb_set_par	= sf_fb_set_par,
	.fb_setcolreg	= sf_fb_setcolreg,
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
	.fb_pan_display	= sf_fb_pan_display,
	.fb_mmap	= sf_fb_mmap,
	.fb_blank	= sf_fb_blank,
};

static int sf_fb_map_video_memory(struct sf_fb_data *sf_dev)
{
	struct resource res_mem;
	struct device_node *node;
	int ret;

	node = of_parse_phandle(sf_dev->dev->of_node, "memory-region", 0);
	if (!node) {
		dev_err(sf_dev->dev, "Could not get reserved memory.\n");
		return -ENOMEM;
	}

	ret = of_address_to_resource(node, 0, &res_mem);
	if (ret)
		return ret;

	sf_dev->fb.screen_size = resource_size(&res_mem);
	sf_dev->fb.fix.smem_start = res_mem.start;

	sf_dev->fb.screen_base = devm_ioremap_resource(sf_dev->dev, &res_mem);
	if (IS_ERR(sf_dev->fb.screen_base))
		return PTR_ERR(sf_dev->fb.screen_base);

	memset(sf_dev->fb.screen_base, 0, sf_dev->fb.screen_size);
	return 0;
}

static int sf_fb_init(struct sf_fb_data *sf_dev)
{
	int ret;

	INIT_LIST_HEAD(&sf_dev->fb.modelist);
	sf_dev->fb.device = sf_dev->dev;
	sf_dev->fb.fbops = &sf_fb_ops;
	sf_dev->fb.flags = FBINFO_DEFAULT;
	sf_dev->fb.node = -1;
	sf_dev->fb.pseudo_palette = sf_dev->pseudo_pal;
	sf_dev->fb.mode = &sf_dev->display_info;

	strcpy(sf_dev->fb.fix.id, STARFIVE_NAME);
	sf_dev->fb.fix.type = FB_TYPE_PACKED_PIXELS;
	sf_dev->fb.fix.type_aux = 0;
	sf_dev->fb.fix.xpanstep = 0;
	sf_dev->fb.fix.ypanstep = 1;
	sf_dev->fb.fix.ywrapstep = 0;
	sf_dev->fb.fix.accel = FB_ACCEL_NONE;

/*	sf_dev->win_x_size = sf_dev->display_info.xres;
	sf_dev->win_y_size = sf_dev->display_info.yres;
	sf_dev->src_width = sf_dev->display_info.xres;
	sf_dev->alpha = 255; // 255 = solid 0 = transparent
	sf_dev->alpha_en = 0;
	sf_dev->input_format = LCDC_LAYER_INPUT_FORMAT_ARGB8888;*/
	sf_dev->buf_num = BUFFER_NUMS;

	sf_fb_get_var(&sf_dev->fb.var, sf_dev);
	sf_dev->fb.var.xres_virtual = sf_dev->display_info.xres;
	sf_dev->fb.var.yres_virtual = sf_dev->display_info.yres * sf_dev->buf_num;
	sf_dev->fb.var.xoffset = 0;
	sf_dev->fb.var.yoffset = 0;
	sf_dev->fb.var.nonstd = 0;
	sf_dev->fb.var.activate = FB_ACTIVATE_NOW;
	sf_dev->fb.var.width = sf_dev->display_dev->width;
	sf_dev->fb.var.height = sf_dev->display_dev->height;
	sf_dev->fb.var.accel_flags = 0;

/*	layer->flags = 0;
	layer->cmap_inverse = 0;
	layer->cmap_static = 0;
*/
	if (sf_dev->display_dev->bpp <= 16 ) {
		/* 8, 16 bpp */
		sf_dev->buf_size = sf_dev->display_info.xres *
			sf_dev->display_info.yres * sf_dev->display_dev->bpp / 8;
	} else {
		/* 18, 32 bpp*/
		sf_dev->buf_size = sf_dev->display_info.xres * sf_dev->display_info.yres * 4;
	}

	sf_dev->fb.fix.smem_len = sf_dev->buf_size * sf_dev->buf_num;

	ret = sf_fb_map_video_memory(sf_dev);
	if (ret) {
		dev_err(sf_dev->dev, "Fail to allocate video RAM\n");
		return ret;
	}

	//layer->buf_addr = layer->map_dma;

	return 0;
}

static int sf_fbinfo_init(struct device *dev, struct sf_fb_data *sf_dev)
{
	struct sf_fb_display_dev *display_dev = NULL;
	int ret;

	display_dev = sf_fb_display_dev_get(sf_dev);
	if (!display_dev) {
		dev_err(sf_dev->dev, "Could not get display dev\n");
	}
	sf_dev->display_dev = display_dev;

	sf_dev->pixclock = display_dev->pclk;

/*	clk_set_rate(sf_dev->mclk, sf_dev->pixclock);
	sf_dev->pixclock = clk_get_rate(sf_dev->mclk);
	dev_info(sf_dev->dev,"sf_dev->pixclock = %d\n", sf_dev->pixclock);
*/
	switch(display_dev->interface_info) {
		case STARFIVEFB_MIPI_IF:
			if (display_dev->timing.mipi.display_mode == MIPI_VIDEO_MODE){
				sf_dev->refresh_en = 1;
				display_dev->refresh_en = 1;
				sf_dev->display_info.name = display_dev->name;
				sf_dev->display_info.xres = display_dev->xres;
				sf_dev->display_info.yres = display_dev->yres;
				sf_dev->display_info.pixclock = 1000000 / (sf_dev->pixclock / 1000000);
				sf_dev->display_info.sync = 0;
				sf_dev->display_info.left_margin = display_dev->timing.mipi.videomode_info.hbp;
				sf_dev->display_info.right_margin = display_dev->timing.mipi.videomode_info.hfp;
				sf_dev->display_info.upper_margin = display_dev->timing.mipi.videomode_info.vbp;
				sf_dev->display_info.lower_margin = display_dev->timing.mipi.videomode_info.vfp;
				sf_dev->display_info.hsync_len = display_dev->timing.mipi.videomode_info.hsync;
				sf_dev->display_info.vsync_len = display_dev->timing.mipi.videomode_info.vsync;
				if (display_dev->timing.mipi.videomode_info.sync_pol == FB_HSYNC_HIGH_ACT)
					sf_dev->display_info.sync = FB_SYNC_HOR_HIGH_ACT;
				if (display_dev->timing.mipi.videomode_info.sync_pol == FB_VSYNC_HIGH_ACT)
					sf_dev->display_info.sync = FB_SYNC_VERT_HIGH_ACT;

				sf_dev->panel_info.name = display_dev->name;
				sf_dev->panel_info.w = display_dev->xres;
				sf_dev->panel_info.h = display_dev->yres;
				sf_dev->panel_info.bpp = display_dev->bpp;
				sf_dev->panel_info.fps = display_dev->timing.mipi.fps;
				sf_dev->panel_info.dpi_pclk = display_dev->pclk;
				sf_dev->panel_info.dpi_hsa = display_dev->timing.mipi.videomode_info.hsync;
				sf_dev->panel_info.dpi_hbp = display_dev->timing.mipi.videomode_info.hbp;
				sf_dev->panel_info.dpi_hfp = display_dev->timing.mipi.videomode_info.hfp;
				sf_dev->panel_info.dpi_vsa = display_dev->timing.mipi.videomode_info.vsync;
				sf_dev->panel_info.dpi_vbp = display_dev->timing.mipi.videomode_info.vbp;
				sf_dev->panel_info.dpi_vfp = display_dev->timing.mipi.videomode_info.vfp;
				sf_dev->panel_info.dphy_lanes = display_dev->timing.mipi.no_lanes;
				sf_dev->panel_info.dphy_bps = display_dev->timing.mipi.dphy_bps;
				sf_dev->panel_info.dsi_burst_mode = display_dev->timing.mipi.dsi_burst_mode;
				sf_dev->panel_info.dsi_sync_pulse = display_dev->timing.mipi.dsi_sync_pulse;
				sf_dev->panel_info.dsi_hsa = display_dev->timing.mipi.dsi_hsa;
				sf_dev->panel_info.dsi_hbp = display_dev->timing.mipi.dsi_hbp;
				sf_dev->panel_info.dsi_hfp = display_dev->timing.mipi.dsi_hfp;
				sf_dev->panel_info.dsi_vsa = display_dev->timing.mipi.dsi_vsa;
				sf_dev->panel_info.dsi_vbp = display_dev->timing.mipi.dsi_vbp;
				sf_dev->panel_info.dsi_vfp = display_dev->timing.mipi.dsi_vfp;
			}else if (display_dev->timing.mipi.display_mode == MIPI_COMMAND_MODE){
				sf_dev->display_info.name = display_dev->name;
				sf_dev->display_info.xres = display_dev->xres;
				sf_dev->display_info.yres = display_dev->yres;
				sf_dev->display_info.pixclock = 1000000 / (sf_dev->pixclock / 1000000);
				sf_dev->display_info.left_margin = 0;
				sf_dev->display_info.right_margin = 0;
				sf_dev->display_info.upper_margin = 0;
				sf_dev->display_info.lower_margin = 0;
				sf_dev->display_info.hsync_len = 0;
				sf_dev->display_info.vsync_len = 0;
				sf_dev->display_info.sync = 0;
			}
			break;
		case STARFIVEFB_RGB_IF:
				sf_dev->refresh_en = 1;
				display_dev->refresh_en = 1;
				sf_dev->display_info.name = display_dev->name;
				sf_dev->display_info.xres = display_dev->xres;
				sf_dev->display_info.yres = display_dev->yres;
				sf_dev->display_info.pixclock = 1000000 / (sf_dev->pixclock / 1000000);
				sf_dev->display_info.sync = 0;
				sf_dev->display_info.left_margin = display_dev->timing.rgb.videomode_info.hbp;
				sf_dev->display_info.right_margin = display_dev->timing.rgb.videomode_info.hfp;
				sf_dev->display_info.upper_margin = display_dev->timing.rgb.videomode_info.vbp;
				sf_dev->display_info.lower_margin = display_dev->timing.rgb.videomode_info.vfp;
				sf_dev->display_info.hsync_len = display_dev->timing.rgb.videomode_info.hsync;
				sf_dev->display_info.vsync_len = display_dev->timing.rgb.videomode_info.vsync;
				if (display_dev->timing.rgb.videomode_info.sync_pol == FB_HSYNC_HIGH_ACT)
					sf_dev->display_info.sync = FB_SYNC_HOR_HIGH_ACT;
				if (display_dev->timing.rgb.videomode_info.sync_pol == FB_VSYNC_HIGH_ACT)
					sf_dev->display_info.sync = FB_SYNC_VERT_HIGH_ACT;
			break;
		default:
			break;
	}

	ret = sf_fb_init(sf_dev);
	if (ret) {
		dev_err(sf_dev->dev, "starfive fb init fail\n");
		return ret;
	}

	return 0;
}

static int stfb_open(struct inode *inode, struct file *file)
{
	int ret = 0;

	mutex_lock(&stf_mutex);
	if (stf_dev != NULL)
		file->private_data = stf_dev;
	else {
		ret = -ENODEV;
		pr_err("stf_dev is NULL !\n");
	}
	mutex_unlock(&stf_mutex);

	return ret;
}

static ssize_t stfb_read(struct file *file, char __user * buf,
			size_t count, loff_t * ppos)
{
	int ret = 1;

	return ret;
}

static int stfb_release(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct vm_operations_struct mmap_mem_ops = {
#ifdef CONFIG_HAVE_IOREMAP_PROT
	.access = generic_access_phys
#endif
};

static int stfb_mmap(struct file *file, struct vm_area_struct *vma)
{
//	struct sf_fb_data *sf_dev = file->private_data;
	size_t size = vma->vm_end - vma->vm_start;
	//unsigned long pfn = sf_dev->fb.fix.smem_start + 0x1000000;
	unsigned long pfn = 0xfc000000;

	vma->vm_ops = &mmap_mem_ops;
	/* Remap-pfn-range will mark the range VM_IO */
	if (remap_pfn_range(vma,
			    vma->vm_start,
			    pfn >> PAGE_SHIFT,
			    size, vma->vm_page_prot)) {
		return -EAGAIN;
	}

	return 0;
}

static long int stfb_ioctl(struct file *file, unsigned int cmd, long unsigned int arg)
{
	return 0;
}


static const struct file_operations stfb_fops = {
	.owner = THIS_MODULE,
	.open = stfb_open,
	.read = stfb_read,
	.release = stfb_release,
	.unlocked_ioctl = stfb_ioctl,
	.mmap = stfb_mmap,
};

static void sf_fb_pp_enable_intr(struct sf_fb_data *sf_dev, int enable) {
	int pp_id;

	for (pp_id = 0; pp_id < PP_NUM; pp_id++) {
		if(1 == sf_dev->pp[pp_id].inited) {
			if (enable) {
				pp_enable_intr(sf_dev, pp_id);
			} else {
				pp_disable_intr(sf_dev, pp_id);
			}
		}
	}
}

static int sf_fb_pp_get_2lcdc_id(struct sf_fb_data *sf_dev) {
	int pp_id;

	for (pp_id = 0; pp_id < PP_NUM; pp_id++) {
		if(1 == sf_dev->pp[pp_id].inited) {
			if ((1 == sf_dev->pp[pp_id].fifo_out)
				&& (0 == sf_dev->pp[pp_id].bus_out)) {
				return pp_id;
			}
		}
	}

	if (pp_id == PP_NUM - 1)
		dev_warn(sf_dev->dev, "NO pp connect to LCDC\n");

	return -ENODEV;
}

static int sf_fb_lcdc_init(struct sf_fb_data *sf_dev) {
	int pp_id;
	int lcd_in_pp;
	int winNum;

	pp_id = sf_dev->pp_conn_lcdc;
	if (pp_id < 0) {
		dev_info(sf_dev->dev, "DDR to LCDC\n");
		lcd_in_pp = LCDC_IN_LCD_AXI;
		winNum = lcdc_win_sel(sf_dev, lcd_in_pp);
		sf_dev->winNum = winNum;
		lcdc_config(sf_dev, winNum);
	} else {
		lcd_in_pp = (pp_id == 0) ? LCDC_IN_VPP0 : ((pp_id == 1) ? LCDC_IN_VPP1 : LCDC_IN_VPP2);
		winNum = lcdc_win_sel(sf_dev, lcd_in_pp);
		sf_dev->winNum = winNum;
		lcdc_config(sf_dev, winNum);
	}

	return 0;
}

static int sf_fb_pp_video_mode_init(struct sf_fb_data *sf_dev, struct pp_video_mode *src,
		struct pp_video_mode *dst, int pp_id) {

	if ((NULL == src) || (NULL == dst)) {
		dev_err(sf_dev->dev, "Invalid argument!\n");
		return -EINVAL;
	}

	if ((pp_id < PP_NUM) && (pp_id >= 0 )) {
		src->format = sf_dev->pp[pp_id].src.format;
		src->width = sf_dev->pp[pp_id].src.width;
		src->height = sf_dev->pp[pp_id].src.height;
#ifndef CONFIG_FRAMEBUFFER_CONSOLE
		src->addr = 0xf9000000;
#else
		src->addr = 0xfb000000;
#endif
		dst->format = sf_dev->pp[pp_id].dst.format;
		dst->width = sf_dev->pp[pp_id].dst.width;
		dst->height = sf_dev->pp[pp_id].dst.height;
		if(true == sf_dev->pp[pp_id].bus_out)	/*out to ddr*/
			dst->addr = 0xfc000000;
		else if (true == sf_dev->pp[pp_id].fifo_out)	/*out to lcdc*/
			dst->addr = 0;
	} else {
		dev_err(sf_dev->dev, "pp_id %d is not support\n", pp_id);
		return -EINVAL;
	}

	return 0;
}

static int sf_fb_pp_init(struct sf_fb_data *sf_dev) {
	int pp_id;
	int ret = 0;
	struct pp_video_mode src, dst;

	for (pp_id = 0; pp_id < PP_NUM; pp_id++) {
		if(1 == sf_dev->pp[pp_id].inited) {
				ret = sf_fb_pp_video_mode_init(sf_dev, &src, &dst, pp_id);
				if (!ret)
					pp_config(sf_dev, pp_id, &src, &dst);
		}
	}

	return ret;
}

static int sf_fb_pp_run(struct sf_fb_data *sf_dev) {
	int pp_id;
	int ret = 0;

	for (pp_id = 0; pp_id < PP_NUM; pp_id++) {
		if(1 == sf_dev->pp[pp_id].inited) {
			pp_run(sf_dev, pp_id, PP_RUN);
		}
	}

	return ret;
}

static int sf_fb_parse_dt(struct device *dev, struct sf_fb_data *sf_dev) {
	int ret;
	struct device_node *np = dev->of_node;
	struct device_node *child;
	int pp_num = 0;

	if(!np)
		return -EINVAL;

	sf_dev->pp = devm_kzalloc(dev, sizeof(struct pp_mode) * PP_NUM, GFP_KERNEL);
	if (!sf_dev->pp) {
		dev_err(dev,"allocate memory for platform data failed\n");
		return -ENOMEM;
	}

	if (of_property_read_u32(np, "ddr-format", &sf_dev->ddr_format)) {
		dev_err(dev,"Missing src-format property in the DT.\n");
		ret = -EINVAL;
	}

#ifndef CONFIG_FB_STARFIVE_VIDEO
    return ret;
#endif

	for_each_child_of_node(np, child) {
		if (of_property_read_u32(child, "pp-id", &pp_num)) {
			dev_err(dev,"Missing pp-id property in the DT.\n");
			ret = -EINVAL;
			continue;
		}
		if (pp_num >= PP_NUM)
			dev_err(dev," pp-id number %d is not support!\n", pp_num);

		sf_dev->pp[pp_num].pp_id = pp_num;
		sf_dev->pp[pp_num].bus_out = of_property_read_bool(child, "sys-bus-out");
		sf_dev->pp[pp_num].fifo_out = of_property_read_bool(child, "fifo-out");
		if (of_property_read_u32(child, "src-format", &sf_dev->pp[pp_num].src.format)) {
			dev_err(dev,"Missing src-format property in the DT.\n");
			ret = -EINVAL;
		}
		if (of_property_read_u32(child, "src-width", &sf_dev->pp[pp_num].src.width)) {
			dev_err(dev,"Missing src-width property in the DT. w %d \n", sf_dev->pp[pp_num].src.width);
			ret = -EINVAL;
		}
		if (of_property_read_u32(child, "src-height", &sf_dev->pp[pp_num].src.height)) {
			dev_err(dev,"Missing src-height property in the DT.\n");
			ret = -EINVAL;
		}
		if (of_property_read_u32(child, "dst-format", &sf_dev->pp[pp_num].dst.format)) {
			dev_err(dev,"Missing dst-format property in the DT.\n");
			ret = -EINVAL;
		}
		if (of_property_read_u32(child, "dst-width", &sf_dev->pp[pp_num].dst.width)) {
			dev_err(dev,"Missing dst-width property in the DT.\n");
			ret = -EINVAL;
		}
		if (of_property_read_u32(child, "dst-height", &sf_dev->pp[pp_num].dst.height)) {
			dev_err(dev,"Missing dst-height property in the DT.\n");
			ret = -EINVAL;
		}
		sf_dev->pp[pp_num].inited = 1;
	}

	return ret;
}

static int starfive_fb_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sf_fb_data *sf_dev;
	int ret;

	dev_info(dev, "%s\n", __func__);

	sf_dev = devm_kzalloc(&pdev->dev, sizeof(struct sf_fb_data), GFP_KERNEL);
	if (!sf_dev)
		return -ENOMEM;

	if (sf_get_mem_res(pdev, sf_dev)) {
		dev_err(dev, "get memory resource FAIL\n");
		return -ENOMEM;
	}

	ret = sf_fb_parse_dt(dev, sf_dev);

#if defined(CONFIG_VIDEO_STARFIVE_VIN)
	sf_dev->vin.notifier_call = vin_frame_complete_notify;
	sf_dev->vin.priority = 0;
	ret = vin_notifier_register(&sf_dev->vin);
	if (ret) {
		return ret;
	}
#endif

#if defined(CONFIG_FB_STARFIVE_HDMI_TDA998X)
	sf_dev->dis_dev_name = "tda_998x_1080p";
#elif defined(CONFIG_FB_STARFIVE_HDMI_ADV7513)
	sf_dev->dis_dev_name = "adv_7513_1080p";
#elif defined(CONFIG_FB_STARFIVE_SEEED5INCH)
	sf_dev->dis_dev_name = "seeed_5_inch";
#else
	dev_err(dev, "no dev name matched\n");
	return -EINVAL;
#endif
	sf_dev->cmap_inverse = 0;
	sf_dev->cmap_static = 0;
	sf_dev->dev = &pdev->dev;
	ret = sf_fbinfo_init(&pdev->dev, sf_dev);
	if (ret) {
		dev_err(dev, "fb info init FAIL\n");
		return ret;
	}

#ifndef CONFIG_FRAMEBUFFER_CONSOLE
	/*the address 0xf9000000 is required in CMA modem by VIN,
	*the case used to check VIN image data path only
	*is not normal application.
	*/
	sf_dev->fb.fix.smem_start = 0xf9000000;
#endif

	sf_dev->lcdc_irq = platform_get_irq_byname(pdev, "lcdc_irq");
	if (sf_dev->lcdc_irq == -EPROBE_DEFER)
		return sf_dev->lcdc_irq;
	if (sf_dev->lcdc_irq < 0) {
		dev_err(dev, "couldn't get lcdc irq\n");
		return sf_dev->lcdc_irq;
	}

	sf_dev->vpp1_irq = platform_get_irq_byname(pdev, "vpp1_irq");
	if (sf_dev->vpp1_irq == -EPROBE_DEFER)
		return sf_dev->vpp1_irq;
	if (sf_dev->vpp1_irq < 0) {
		dev_err(dev, "couldn't get vpp1 irq\n");
		return sf_dev->vpp1_irq;
	}

	lcdc_disable_intr(sf_dev);
	sf_fb_pp_enable_intr(sf_dev, PP_INTR_DISABLE);

	ret = devm_request_irq(&pdev->dev, sf_dev->lcdc_irq, lcdc_isr_handler, 0,
			       "sf_lcdc", sf_dev);
	if (ret) {
		dev_err(&pdev->dev, "failure requesting irq %i: %d\n",
			sf_dev->lcdc_irq, ret);
		return ret;
	}

	ret = devm_request_irq(&pdev->dev, sf_dev->vpp1_irq, vpp1_isr_handler, 0,
			       "sf_vpp1", sf_dev);
	if (ret) {
		dev_err(&pdev->dev, "failure requesting irq %i: %d\n",
			sf_dev->vpp1_irq, ret);
		return ret;
	}


    if(STARFIVEFB_MIPI_IF == sf_dev->display_dev->interface_info){
		lcdc_dsi_sel(sf_dev);
		sf_mipi_init(sf_dev);
    }
	if (sf_fb_pp_init(sf_dev)) {
		dev_err(dev, "pp init fail\n");
		return -ENODEV;
	}
	//pp_run(sf_dev, PP_ID_1, PP_RUN);
	//pp_run(sf_dev, PP_ID_0, PP_RUN);
	sf_fb_pp_run(sf_dev);

	if (sf_fb_lcdc_clk_cfg(sf_dev)) {
		dev_err(dev, "lcdc clock configure fail\n");
		return -EINVAL;
	}
	sf_dev->pp_conn_lcdc = sf_fb_pp_get_2lcdc_id(sf_dev);
	if (sf_fb_lcdc_init(sf_dev)) {
		dev_err(dev, "lcdc init fail\n");
		return -EINVAL;
	}
	lcdc_run(sf_dev, sf_dev->winNum, LCDC_RUN);

	stf_dev = sf_dev;

	platform_set_drvdata(pdev, sf_dev);
	ret = register_framebuffer(&sf_dev->fb);
	if (ret < 0) {
		dev_err(&pdev->dev,"register framebuffer FAIL\n");
		return ret;
	}

	sf_dev->stfbcdev.minor = MISC_DYNAMIC_MINOR;
	sf_dev->stfbcdev.parent = &pdev->dev;
	sf_dev->stfbcdev.name = "stfbcdev";
	sf_dev->stfbcdev.fops = &stfb_fops;

	ret = misc_register(&sf_dev->stfbcdev);
	if (ret) {
		dev_err(dev, "creare stfbcdev FAIL!\n");
		return ret;
	}

	lcdc_enable_intr(sf_dev);
	sf_fb_pp_enable_intr(sf_dev, PP_INTR_ENABLE);

	return 0;
}

static int starfive_fb_remove(struct platform_device *pdev)
{
	struct sf_fb_data *sf_dev = platform_get_drvdata(pdev);

	if(NULL == sf_dev) {
		dev_err(&pdev->dev,"get sf_dev fail\n");
	}

	misc_deregister(&sf_dev->stfbcdev);

	return 0;
}

static void starfive_fb_shutdown(struct platform_device *dev)
{
	return ;
}

static struct of_device_id starfive_fb_dt_match[] = {
	{
		.compatible = "starfive,vpp-lcdc",
	},
	{}
};

static struct platform_driver starfive_fb_driver = {
	.probe		= starfive_fb_probe,
	.remove		= starfive_fb_remove,
	.shutdown	= starfive_fb_shutdown,
	.driver		= {
		.name	= "starfive,vpp-lcdc",
		.of_match_table = starfive_fb_dt_match,
	},
};

static int __init starfive_fb_init(void)
{
	return platform_driver_register(&starfive_fb_driver);
}

static void __exit starfive_fb_cleanup(void)
{
	platform_driver_unregister(&starfive_fb_driver);
}

module_init(starfive_fb_init);
module_exit(starfive_fb_cleanup);

MODULE_AUTHOR("StarFive Technology Co., Ltd.");
MODULE_DESCRIPTION("loadable LCDC&VPP driver for StarFive");
MODULE_LICENSE("GPL");
