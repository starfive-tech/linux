// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 StarFive Technology Co., Ltd.
 */
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/reset.h>
#include <linux/delay.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_atomic_uapi.h>
#include <drm/drm_fb_cma_helper.h>
#include <drm/drm_print.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_vblank.h>
#include <drm/drm_gem_atomic_helper.h>
#include "starfive_drm_drv.h"
#include "starfive_drm_crtc.h"
#include "starfive_drm_plane.h"
#include "starfive_drm_lcdc.h"
#include "starfive_drm_vpp.h"
//#include <video/sys_comm_regs.h>

static inline struct drm_encoder *
starfive_head_atom_get_encoder(struct starfive_crtc *sf_crtc)
{
	struct drm_encoder *encoder = NULL;

	/* We only ever have a single encoder */
	drm_for_each_encoder_mask(encoder, sf_crtc->crtc.dev,
				  sf_crtc->crtc.state->encoder_mask)
		break;

	return encoder;
}

static int ddrfmt_to_ppfmt(struct starfive_crtc *sf_crtc)
{
	int ddrfmt = sf_crtc->ddr_format;
	int ret = 0;

	sf_crtc->lcdcfmt = WIN_FMT_xRGB8888; //lcdc default used
	sf_crtc->pp_conn_lcdc = 1;//default config
	switch (ddrfmt) {
	case DRM_FORMAT_UYVY:
		sf_crtc->vpp_format = COLOR_YUV422_UYVY;
		break;
	case DRM_FORMAT_VYUY:
		sf_crtc->vpp_format = COLOR_YUV422_VYUY;
		break;
	case DRM_FORMAT_YUYV:
		sf_crtc->vpp_format = COLOR_YUV422_YUYV;
		break;
	case DRM_FORMAT_YVYU:
		sf_crtc->vpp_format = COLOR_YUV422_YVYU;
		break;
	case DRM_FORMAT_YUV420:
		sf_crtc->vpp_format = COLOR_YUV420P;
		break;
	case DRM_FORMAT_NV21:
		sf_crtc->vpp_format = COLOR_YUV420_NV21;
		break;
	case DRM_FORMAT_NV12:
		sf_crtc->vpp_format = COLOR_YUV420_NV12;
		break;
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_ARGB8888:
		sf_crtc->vpp_format = COLOR_RGB888_ARGB;
		break;
	case DRM_FORMAT_ABGR8888:
		sf_crtc->vpp_format = COLOR_RGB888_ABGR;
		break;
	case DRM_FORMAT_RGBA8888:
		sf_crtc->vpp_format = COLOR_RGB888_RGBA;
		break;
	case DRM_FORMAT_BGRA8888:
		sf_crtc->vpp_format = COLOR_RGB888_BGRA;
		break;
	case DRM_FORMAT_RGB565:
		sf_crtc->vpp_format = COLOR_RGB565;
		//sf_crtc->lcdcfmt = WIN_FMT_RGB565;
		//this format no need pp, lcdc can direct read ddr buff
		//sf_crtc->pp_conn_lcdc = -1;
		break;
	case DRM_FORMAT_XRGB1555:
		sf_crtc->lcdcfmt = WIN_FMT_xRGB1555;
		sf_crtc->pp_conn_lcdc = -1;//this format no need pp, lcdc can direct read ddr buff;
		break;
	case DRM_FORMAT_XRGB4444:
		sf_crtc->lcdcfmt = WIN_FMT_xRGB4444;
		sf_crtc->pp_conn_lcdc = -1;//this format no need pp, lcdc can direct read ddr buff;
		break;

	default:
		ret = -1;
		break;
	}

	return ret;
}

void starfive_crtc_hw_config_simple(struct starfive_crtc *starfive_crtc)
{

}

static void starfive_crtc_destroy(struct drm_crtc *crtc)
{
	drm_crtc_cleanup(crtc);
}

static void starfive_crtc_destroy_state(struct drm_crtc *crtc,
					struct drm_crtc_state *state)
{
	struct starfive_crtc_state *s = to_starfive_crtc_state(state);

	__drm_atomic_helper_crtc_destroy_state(&s->base);
	kfree(s);
}

static void starfive_crtc_reset(struct drm_crtc *crtc)
{
	struct starfive_crtc_state *crtc_state =
	kzalloc(sizeof(*crtc_state), GFP_KERNEL);

	if (crtc->state)
		starfive_crtc_destroy_state(crtc, crtc->state);

	__drm_atomic_helper_crtc_reset(crtc, &crtc_state->base);
}

static struct drm_crtc_state *starfive_crtc_duplicate_state(struct drm_crtc *crtc)
{
	struct starfive_crtc_state *starfive_state;

	starfive_state = kzalloc(sizeof(*starfive_state), GFP_KERNEL);
	if (!starfive_state)
		return NULL;

	__drm_atomic_helper_crtc_duplicate_state(crtc, &starfive_state->base);

	return &starfive_state->base;
}

static int starfive_crtc_enable_vblank(struct drm_crtc *crtc)
{
	//need set hw
	return 0;
}

static void starfive_crtc_disable_vblank(struct drm_crtc *crtc)
{
	//need set hw
}

static const struct drm_crtc_funcs starfive_crtc_funcs = {
	.set_config = drm_atomic_helper_set_config,
	.page_flip = drm_atomic_helper_page_flip,
	.destroy = starfive_crtc_destroy,
	.set_property = NULL,
	.cursor_set = NULL, /* handled by drm_mode_cursor_universal */
	.cursor_move = NULL, /* handled by drm_mode_cursor_universal */
	.reset = starfive_crtc_reset,
	.atomic_duplicate_state = starfive_crtc_duplicate_state,
	.atomic_destroy_state = starfive_crtc_destroy_state,
	//.gamma_set = drm_atomic_helper_legacy_gamma_set,
	.enable_vblank = starfive_crtc_enable_vblank,
	.disable_vblank = starfive_crtc_disable_vblank,
	//.set_crc_source = starfive_crtc_set_crc_source,
	//.get_vblank_timestamp = drm_crtc_vblank_helper_get_vblank_timestamp,
	//.verify_crc_source = starfive_crtc_verify_crc_source,
};

static bool starfive_crtc_mode_fixup(struct drm_crtc *crtc,
				     const struct drm_display_mode *mode,
				     struct drm_display_mode *adjusted_mode)
{
	/* Nothing to do here, but this callback is mandatory. */
	return true;
}

static int starfive_crtc_atomic_check(struct drm_crtc *crtc,
				      struct drm_atomic_state *state)
{
	//state->no_vblank = true;	// hardware without VBLANK interrupt ???
	struct drm_crtc_state *crtc_state = drm_atomic_get_new_crtc_state(state,
										crtc);
	crtc_state->no_vblank = true;

	return 0;
}

static void starfive_crtc_atomic_begin(struct drm_crtc *crtc,
				       struct drm_atomic_state *old_crtc_state)
{
	//starfive_crtc_gamma_set(crtcp, crtc, old_crtc_state);
}

static void starfive_crtc_atomic_flush(struct drm_crtc *crtc,
				       struct drm_atomic_state *old_crtc_state)
{
	struct starfive_crtc *crtcp = to_starfive_crtc(crtc);

	//starfive_flush_dcache(crtcp->dma_addr, 1920*1080*2);
	DRM_DEBUG_DRIVER("ddr_format_change [%d], dma_addr_change [%d]\n",
			 crtcp->ddr_format_change, crtcp->dma_addr_change);
	if (crtcp->ddr_format_change || crtcp->dma_addr_change) {
		ddrfmt_to_ppfmt(crtcp);
		starfive_pp_update(crtcp);
	} else {
		DRM_DEBUG_DRIVER("%s with no change\n", __func__);
	}
}

static void starfive_crtc_atomic_enable(struct drm_crtc *crtc,
					struct drm_atomic_state *state)
{
	struct starfive_crtc *crtcp = to_starfive_crtc(crtc);

// enable crtc HW
#ifdef CONFIG_DRM_STARFIVE_MIPI_DSI
	dsitx_vout_init(crtcp);
	lcdc_dsi_sel(crtcp);
#else
	vout_reset(crtcp);
#endif
	ddrfmt_to_ppfmt(crtcp);
	starfive_pp_enable(crtcp);
	starfive_lcdc_enable(crtcp);
	crtcp->is_enabled = true;  // should before
}

static void starfive_crtc_atomic_disable(struct drm_crtc *crtc,
					 struct drm_atomic_state *state)
{
	struct starfive_crtc *crtcp = to_starfive_crtc(crtc);
	int pp_id;

	for (pp_id = 0; pp_id < PP_NUM; pp_id++) {
		if (crtcp->pp[pp_id].inited == 1) {
			pp_disable_intr(crtcp, pp_id);
			vout_disable(crtcp); // disable crtc HW
		}
	}
	crtcp->is_enabled = false;
}

static enum drm_mode_status starfive_crtc_mode_valid(struct drm_crtc *crtc,
						     const struct drm_display_mode *mode)
{
	int refresh = drm_mode_vrefresh(mode);

	if (refresh > 60) //lcdc miss support 60+ fps
		return MODE_BAD;
	else
		return MODE_OK;
}

static const struct drm_crtc_helper_funcs starfive_crtc_helper_funcs = {
	.mode_fixup = starfive_crtc_mode_fixup,
	.atomic_check = starfive_crtc_atomic_check,
	.atomic_begin = starfive_crtc_atomic_begin,
	.atomic_flush = starfive_crtc_atomic_flush,
	.atomic_enable = starfive_crtc_atomic_enable,
	.atomic_disable = starfive_crtc_atomic_disable,
	.mode_valid = starfive_crtc_mode_valid,
};

static int starfive_crtc_create(struct drm_device *drm_dev,
				struct starfive_crtc *starfive_crtc,
				const struct drm_crtc_funcs *crtc_funcs,
				const struct drm_crtc_helper_funcs *crtc_helper_funcs)
{
	struct drm_crtc *crtc = &starfive_crtc->crtc;
	struct device *dev = drm_dev->dev;
	struct device_node *port;
	int ret;

	starfive_crtc->planes = devm_kzalloc(dev, sizeof(struct drm_plane), GFP_KERNEL);
	ret = starfive_plane_init(drm_dev, starfive_crtc, DRM_PLANE_TYPE_PRIMARY);
	if (ret) {
		dev_err(drm_dev->dev, "failed to construct primary plane\n");
		return ret;
	}

	drm_crtc_init_with_planes(drm_dev, crtc, starfive_crtc->planes, NULL,
				  crtc_funcs, NULL);
	drm_crtc_helper_add(crtc, crtc_helper_funcs);
	port = of_get_child_by_name(starfive_crtc->dev->of_node, "port");
	if (!port) {
		DRM_ERROR("no port node found in %s\n", dev->of_node->full_name);
		ret = -ENOENT;
	}

	crtc->port = port;
	return ret;
}

static int starfive_crtc_get_memres(struct platform_device *pdev, struct starfive_crtc *sf_crtc)
{
	static const char *const mem_res_name[] = {
		"lcdc", "vpp0", "vpp1", "vpp2", "clk", "rst", "sys"
	};
	int i;

	for (i = 0; i < ARRAY_SIZE(mem_res_name); i++) {
		const char *name = mem_res_name[i];
		void __iomem *regs = devm_platform_ioremap_resource_byname(pdev, name);

		if (IS_ERR(regs))
			return PTR_ERR(regs);

		if (!strcmp(name, "lcdc"))
			sf_crtc->base_lcdc = regs;
		else if (!strcmp(name, "vpp0"))
			sf_crtc->base_vpp0 = regs;
		else if (!strcmp(name, "vpp1"))
			sf_crtc->base_vpp1 = regs;
		else if (!strcmp(name, "vpp2"))
			sf_crtc->base_vpp2 = regs;
		else if (!strcmp(name, "clk"))
			sf_crtc->base_clk = regs;
		else if (!strcmp(name, "rst"))
			sf_crtc->base_rst = regs;
		else if (!strcmp(name, "sys"))
			sf_crtc->base_syscfg = regs;
		else
			dev_err(&pdev->dev, "Could not match resource name\n");
	}

	return 0;
}

static int starfive_parse_dt(struct device *dev, struct starfive_crtc *sf_crtc)
{
	int ret;
	struct device_node *np = dev->of_node;
	struct device_node *child;
	int pp_num = 0;

	if (!np)
		return -EINVAL;

	sf_crtc->pp = devm_kzalloc(dev, sizeof(struct pp_mode) * PP_NUM, GFP_KERNEL);
	if (!sf_crtc->pp)
		return -ENOMEM;

	for_each_child_of_node(np, child) {
		if (of_property_read_u32(child, "pp-id", &pp_num)) {
			ret = -EINVAL;
			continue;
		}
		if (pp_num >= PP_NUM)
			dev_err(dev, " pp-id number %d is not support!\n", pp_num);

		sf_crtc->pp[pp_num].pp_id = pp_num;
		sf_crtc->pp[pp_num].bus_out = of_property_read_bool(child, "sys-bus-out");
		sf_crtc->pp[pp_num].fifo_out = of_property_read_bool(child, "fifo-out");
		if (of_property_read_u32(child, "src-format", &sf_crtc->pp[pp_num].src.format)) {
			dev_err(dev, "Missing src-format property in the DT.\n");
			ret = -EINVAL;
		}
		if (of_property_read_u32(child, "src-width", &sf_crtc->pp[pp_num].src.width)) {
			dev_err(dev, "Missing src-width property in the DT. w %d\n",
				sf_crtc->pp[pp_num].src.width);
			ret = -EINVAL;
		}
		if (of_property_read_u32(child, "src-height", &sf_crtc->pp[pp_num].src.height)) {
			dev_err(dev, "Missing src-height property in the DT.\n");
			ret = -EINVAL;
		}
		if (of_property_read_u32(child, "dst-format", &sf_crtc->pp[pp_num].dst.format)) {
			dev_err(dev, "Missing dst-format property in the DT.\n");
			ret = -EINVAL;
		}
		if (of_property_read_u32(child, "dst-width", &sf_crtc->pp[pp_num].dst.width)) {
			dev_err(dev, "Missing dst-width property in the DT.\n");
			ret = -EINVAL;
		}
		if (of_property_read_u32(child, "dst-height", &sf_crtc->pp[pp_num].dst.height)) {
			dev_err(dev, "Missing dst-height property in the DT.\n");
			ret = -EINVAL;
		}
		sf_crtc->pp[pp_num].inited = 1;
	}

	return ret;
}

static int starfive_crtc_bind(struct device *dev, struct device *master, void *data)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct drm_device *drm_dev = data;
	struct starfive_crtc *crtcp;
	int ret;

	crtcp = devm_kzalloc(dev, sizeof(*crtcp), GFP_KERNEL);
	if (!crtcp)
		return -ENOMEM;

	crtcp->dev = dev;
	crtcp->drm_dev = drm_dev;
	dev_set_drvdata(dev, crtcp);

	spin_lock_init(&crtcp->reg_lock);

	ret = starfive_crtc_get_memres(pdev, crtcp);
	if (ret)
		return ret;

	crtcp->clk_disp_axi = devm_clk_get(dev, "disp_axi");
	if (IS_ERR(crtcp->clk_disp_axi))
		return dev_err_probe(dev, PTR_ERR(crtcp->clk_disp_axi),
				     "error getting axi clock\n");

	crtcp->clk_vout_src = devm_clk_get(dev, "disp_axi");
	if (IS_ERR(crtcp->clk_vout_src))
		return dev_err_probe(dev, PTR_ERR(crtcp->clk_vout_src),
				     "error getting vout clock\n");

	crtcp->rst_disp_axi = devm_reset_control_get_exclusive(dev, "disp_axi");
	if (IS_ERR(crtcp->rst_disp_axi))
		return dev_err_probe(dev, PTR_ERR(crtcp->rst_disp_axi),
				     "error getting axi reset\n");

	crtcp->rst_vout_src = devm_reset_control_get_exclusive(dev, "vout_src");
	if (IS_ERR(crtcp->rst_vout_src))
		return dev_err_probe(dev, PTR_ERR(crtcp->rst_vout_src),
				     "error getting vout reset\n");

	ret = starfive_parse_dt(dev, crtcp);

	crtcp->pp_conn_lcdc = starfive_pp_get_2lcdc_id(crtcp);

	crtcp->lcdc_irq = platform_get_irq_byname(pdev, "lcdc_irq");
	if (crtcp->lcdc_irq < 0)
		return dev_err_probe(dev, crtcp->lcdc_irq, "error getting lcdc irq\n");

	crtcp->vpp1_irq = platform_get_irq_byname(pdev, "vpp1_irq");
	if (crtcp->vpp1_irq < 0)
		return dev_err_probe(dev, crtcp->vpp1_irq, "error getting vpp1 irq\n");

	ret = devm_request_irq(&pdev->dev, crtcp->lcdc_irq, lcdc_isr_handler, 0,
			       "sf_lcdc", crtcp);
	if (ret)
		return dev_err_probe(dev, ret, "error requesting irq %d\n", crtcp->lcdc_irq);

	ret = devm_request_irq(&pdev->dev, crtcp->vpp1_irq, vpp1_isr_handler, 0,
			       "sf_vpp1", crtcp);
	if (ret)
		return dev_err_probe(dev, ret, "error requesting irq %d\n", crtcp->vpp1_irq);

	ret = starfive_crtc_create(drm_dev, crtcp,
				   &starfive_crtc_funcs,
				   &starfive_crtc_helper_funcs);
	if (ret)
		return ret;

	crtcp->is_enabled = false;

	/* starfive_set_crtc_possible_masks(drm_dev, crtcp); */

	/*
	ret = drm_self_refresh_helper_init(crtcp);
	if (ret)
		DRM_DEV_DEBUG_KMS(crtcp->dev,
			"Failed to init %s with SR helpers %d, ignoring\n",
			crtcp->name, ret);
	*/

	return 0;
}

static void starfive_crtc_unbind(struct device *dev, struct device *master, void *data)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct starfive_crtc *crtcp = dev_get_drvdata(dev);

	drm_crtc_cleanup(&crtcp->crtc);
	platform_set_drvdata(pdev, NULL);
}

static const struct component_ops starfive_crtc_component_ops = {
	.bind   = starfive_crtc_bind,
	.unbind = starfive_crtc_unbind,
};

static const struct of_device_id starfive_crtc_driver_dt_match[] = {
	{ .compatible = "starfive,jh7100-crtc" },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, starfive_crtc_driver_dt_match);

static int starfive_crtc_probe(struct platform_device *pdev)
{
	return component_add(&pdev->dev, &starfive_crtc_component_ops);
}

static int starfive_crtc_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &starfive_crtc_component_ops);
	return 0;
}

struct platform_driver starfive_crtc_driver = {
	.probe = starfive_crtc_probe,
	.remove = starfive_crtc_remove,
	.driver = {
		.name = "starfive-crtc",
		.of_match_table = of_match_ptr(starfive_crtc_driver_dt_match),
	},
};
