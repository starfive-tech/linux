/* driver/video/starfive/starfive_displayer.c
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
**	0.1.0		2020-11-10	starfive		created
**
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/compiler.h>
#include <linux/init.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/err.h>

#include "starfive_fb.h"
#include "starfive_display_dev.h"

#ifdef _ALIGN_UP
#undef _ALIGN_UP
#define _ALIGN_UP(addr, size) (((addr)+((size)-1))&(~((typeof(addr))(size)-1)))
#endif

#define DSI_CMD_LEN(hdr)	(sizeof(*hdr) + (hdr)->dlen)

static int sf_displayer_reset(struct sf_fb_data *fbi)
{
	return 0;
}

static int sf_displayer_power_on(struct sf_fb_data *fbi, int onoff)
{
	return 0;
}

static int sf_displayer_suspend(struct sf_fb_data *fbi)
{
	return 0;
}

static int sf_displayer_resume(struct sf_fb_data *fbi)
{
	return 0;
}

static void __maybe_unused dump_panel_info(struct device *dev,
                           struct sf_fb_display_dev *dev_data)
{

	dev_dbg(dev, "id info: pack_type = 0x%x, cmd = 0x%x, id_count = %d, id = 0x%x, 0x%x\n",
			dev_data->panel_id_info.id_info->hdr.pack_type,
			dev_data->panel_id_info.id_info->hdr.cmd,
			dev_data->panel_id_info.id_info->hdr.id_count,
			dev_data->panel_id_info.id_info->id[0],
			dev_data->panel_id_info.id_info->id[1]);

}

static int of_parse_video_mode(struct device_node *np,
                           struct video_mode_info *videomode_info)
{
	int rc;
	u32 temp_val;
	const char *data;

	rc = of_property_read_u32(np, "h-pulse-width", &temp_val);
	if (rc) {
		pr_err("%s:%d, h-pulse-width not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	videomode_info->hsync= temp_val;
	rc = of_property_read_u32(np, "h-back-porch", &temp_val);
	if (rc) {
		pr_err("%s:%d, h-back-porch not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	videomode_info->hbp = temp_val;
	rc = of_property_read_u32(np, "h-front-porch", &temp_val);
	if (rc) {
		pr_err("%s:%d, h-front-porch not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	videomode_info->hfp = temp_val;

	rc = of_property_read_u32(np, "v-pulse-width", &temp_val);
	if (rc) {
		pr_err("%s:%d, v-pulse-width not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	videomode_info->vsync = temp_val;
	rc = of_property_read_u32(np, "v-back-porch", &temp_val);
	if (rc) {
		pr_err("%s:%d, v-back-porch not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	videomode_info->vbp = temp_val;
	rc = of_property_read_u32(np, "v-front-porch", &temp_val);
	if (rc) {
		pr_err("%s:%d, v-front-porch not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	videomode_info->vfp = temp_val;

	videomode_info->sync_pol = FB_VSYNC_HIGH_ACT;
	data = of_get_property(np, "sync_pol", NULL);
	if (data) {
		if (!strcmp(data, "vsync_high_act"))
			videomode_info->sync_pol = FB_VSYNC_HIGH_ACT;
		else if (!strcmp(data, "hsync_high_act"))
			videomode_info->sync_pol = FB_HSYNC_HIGH_ACT;
	}

	videomode_info->lp_cmd_en = of_property_read_bool(np,
					"lp_cmd_en");
	videomode_info->lp_hfp_en = of_property_read_bool(np,
					"lp_hfp_en");
	videomode_info->lp_hbp_en = of_property_read_bool(np,
					"lp_hbp_en");
	videomode_info->lp_vact_en = of_property_read_bool(np,
					"lp_vact_en");
	videomode_info->lp_vfp_en = of_property_read_bool(np,
					"lp_vfp_en");
	videomode_info->lp_vbp_en = of_property_read_bool(np,
					"lp_vbp_en");
	videomode_info->lp_vsa_en = of_property_read_bool(np,
					"lp_vsa_en");

	videomode_info->mipi_trans_type = VIDEO_BURST_WITH_SYNC_PULSES;
	data = of_get_property(np, "traffic-mode", NULL);
	if (data) {
		if (!strcmp(data, "burst_with_sync_pulses"))
			videomode_info->sync_pol = VIDEO_BURST_WITH_SYNC_PULSES;
		else if (!strcmp(data, "non_burst_with_sync_pulses"))
			videomode_info->sync_pol = VIDEO_NON_BURST_WITH_SYNC_PULSES;
		else if (!strcmp(data, "non_burst_with_sync_events"))
			videomode_info->sync_pol = VIDEO_NON_BURST_WITH_SYNC_EVENTS;
	}

	return 0;
}

static int of_parse_command_mode(struct device_node *np,
                           struct command_mode_info *cmdmode_info)
{
	int rc;
	u32 temp_val;

	cmdmode_info->tear_fx_en = of_property_read_bool(np,
					"tear_fx_en");
	cmdmode_info->ack_rqst_en = of_property_read_bool(np,
					"ack_rqst_en");
	cmdmode_info->gen_sw_0p_tx = of_property_read_bool(np,
					"gen_sw_0p_tx");
	cmdmode_info->gen_sw_1p_tx = of_property_read_bool(np,
					"gen_sw_1p_tx");
	cmdmode_info->gen_sw_2p_tx = of_property_read_bool(np,
					"gen_sw_2p_tx");
	cmdmode_info->gen_sr_0p_tx = of_property_read_bool(np,
					"gen_sr_0p_tx");
	cmdmode_info->gen_sr_1p_tx = of_property_read_bool(np,
					"gen_sr_1p_tx");
	cmdmode_info->gen_sr_2p_tx = of_property_read_bool(np,
					"gen_sr_2p_tx");
	cmdmode_info->gen_lw_tx = of_property_read_bool(np,
					"gen_lw_tx");
	cmdmode_info->dcs_sw_0p_tx = of_property_read_bool(np,
					"dcs_sw_0p_tx");
	cmdmode_info->dcs_sw_1p_tx = of_property_read_bool(np,
					"dcs_sw_1p_tx");
	cmdmode_info->dcs_sr_0p_tx = of_property_read_bool(np,
					"dcs_sr_0p_tx");
	cmdmode_info->dcs_lw_tx = of_property_read_bool(np,
					"dcs_lw_tx");
	cmdmode_info->max_rd_pkt_size = of_property_read_bool(np,
					"max_rd_pkt_size");


	rc = of_property_read_u32(np, "hs_rd_to_cnt", &temp_val);
	if (rc) {
		pr_err("%s:%d, hs_rd_to_cnt not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	cmdmode_info->timeout.hs_rd_to_cnt = temp_val;

	rc = of_property_read_u32(np, "lp_rd_to_cnt", &temp_val);
	if (rc) {
		pr_err("%s:%d, lp_rd_to_cnt not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	cmdmode_info->timeout.lp_rd_to_cnt = temp_val;

	rc = of_property_read_u32(np, "hs_wr_to_cnt", &temp_val);
	if (rc) {
		pr_err("%s:%d, hs_wr_to_cnt not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	cmdmode_info->timeout.hs_wr_to_cnt = temp_val;

	rc = of_property_read_u32(np, "lp_wr_to_cnt", &temp_val);
	if (rc) {
		pr_err("%s:%d, lp_wr_to_cnt not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	cmdmode_info->timeout.lp_wr_to_cnt = temp_val;

	rc = of_property_read_u32(np, "bta_to_cnt", &temp_val);
	if (rc) {
		pr_err("%s:%d, bta_to_cnt not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	cmdmode_info->timeout.bta_to_cnt = temp_val;

	return 0;

}

static int of_parse_phy_timing(struct device_node *np,
                           struct phy_time_info *phy_timing)
{
	int rc;
	u8 temp_val;

	rc = of_property_read_u8(np, "data_tprepare", &temp_val);
	if (rc) {
		pr_err("%s:%d, data_tprepare not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	phy_timing->data_tprepare = (!rc ? temp_val : 0);

	rc = of_property_read_u8(np, "data_hs_zero", &temp_val);
	if (rc) {
		pr_err("%s:%d, data_hs_zero not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	phy_timing->data_hs_zero = temp_val;

	rc = of_property_read_u8(np, "data_hs_exit", &temp_val);
	if (rc) {
		pr_err("%s:%d, data_hs_exit not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	phy_timing->data_hs_exit = temp_val;

	rc = of_property_read_u8(np, "data_hs_trail", &temp_val);
	if (rc) {
		pr_err("%s:%d, data_hs_trail not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	phy_timing->data_hs_trail = temp_val;

	return 0;
}

static int of_parse_te_info(struct device_node *np,
                           struct te_info *teinfo)
{
	int rc;
	u32 temp_val;
	const char *data;

	teinfo->te_source = 1;
	data = of_get_property(np, "te_source", NULL);
	if (data) {
		if (!strcmp(data, "external_pin"))
			teinfo->te_source = 1;
		else if (!strcmp(data, "dsi_te_trigger"))
			teinfo->te_source = 0;
	}

	teinfo->te_trigger_mode = 1;
	data = of_get_property(np, "te_trigger_mode", NULL);
	if (data) {
		if (!strcmp(data, "rising_edge"))
			teinfo->te_source = 0;
		else if (!strcmp(data, "high_1000us"))
			teinfo->te_source = 1;
	}

	rc = of_property_read_u32(np, "te_enable", &temp_val);
	if (rc) {
		pr_err("%s:%d, te_enable not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	teinfo->te_en = temp_val;

	rc = of_property_read_u32(np, "cm_te_effect_sync_enable", &temp_val);
	if (rc) {
		pr_err("%s:%d, cm_te_effect_sync_enable not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	teinfo->te_sync_en = temp_val;

	rc = of_property_read_u32(np, "te_count_per_sec", &temp_val);
	if (rc) {
		pr_err("%s:%d, te_count_per_sec not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	teinfo->te_cps = temp_val;

	return 0;
}

static int of_parse_ext_info(struct device_node *np,
                           struct external_info *ext_info)
{
	int rc;
	u32 temp_val;

	ext_info->crc_rx_en = of_property_read_bool(np, "crc_rx_en");
	ext_info->ecc_rx_en = of_property_read_bool(np, "ecc_rx_en");
	ext_info->eotp_rx_en = of_property_read_bool(np, "eotp_rx_en");
	ext_info->eotp_tx_en = of_property_read_bool(np, "eotp_tx_en");

	rc = of_property_read_u32(np, "dev_read_time", &temp_val);
	if (rc) {
		pr_err("%s:%d, dev_read_time not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	ext_info->dev_read_time = temp_val;

	return 0;
}


static int of_parse_mipi_timing(struct device_node *np,
                           struct sf_fb_timing_mipi *mipi_timing)
{
	int rc;
	u32 temp_val;
	const char *data;

	rc = of_property_read_u32(np, "mipi-byte-clock", &temp_val);
	if (rc) {
		pr_err("%s:%d, mipi-byte-clock not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	mipi_timing->hs_freq = temp_val;

	rc = of_property_read_u32(np, "mipi-escape-clock", &temp_val);
	if (rc) {
		pr_err("%s:%d, mipi-escape-clock not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	mipi_timing->lp_freq= temp_val;

	rc = of_property_read_u32(np, "lane-no", &temp_val);
	if (rc) {
		pr_err("%s:%d, lane-no not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	mipi_timing->no_lanes= temp_val;

	rc = of_property_read_u32(np, "fps", &temp_val);
	if (rc) {
		pr_err("%s:%d, fps not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	mipi_timing->fps = temp_val;

	rc = of_property_read_u32(np, "dphy_bps", &temp_val);
	if (rc) {
		pr_err("%s:%d, dphy_bps not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	mipi_timing->dphy_bps = temp_val;

	rc = of_property_read_u32(np, "dsi_burst_mode", &temp_val);
	if (rc) {
		pr_err("%s:%d, dsi_burst_mode not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	mipi_timing->dsi_burst_mode = temp_val;

	rc = of_property_read_u32(np, "dsi_sync_pulse", &temp_val);
	if (rc) {
		pr_err("%s:%d, dsi_sync_pulse not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	mipi_timing->dsi_sync_pulse = temp_val;

	rc = of_property_read_u32(np, "dsi_hsa", &temp_val);
	if (rc) {
		pr_err("%s:%d, dsi_hsa not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	mipi_timing->dsi_hsa = temp_val;

	rc = of_property_read_u32(np, "dsi_hbp", &temp_val);
	if (rc) {
		pr_err("%s:%d, dsi_hbp not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	mipi_timing->dsi_hbp = temp_val;

	rc = of_property_read_u32(np, "dsi_hfp", &temp_val);
	if (rc) {
		pr_err("%s:%d, dsi_hfp not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	mipi_timing->dsi_hfp = temp_val;

	rc = of_property_read_u32(np, "dsi_vsa", &temp_val);
	if (rc) {
		pr_err("%s:%d, dsi_vsa not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	mipi_timing->dsi_vsa = temp_val;

	rc = of_property_read_u32(np, "dsi_vbp", &temp_val);
	if (rc) {
		pr_err("%s:%d, dsi_vbp not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	mipi_timing->dsi_vbp = temp_val;

	rc = of_property_read_u32(np, "dsi_vfp", &temp_val);
	if (rc) {
		pr_err("%s:%d, dsi_vfp not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	mipi_timing->dsi_vfp = temp_val;

	/*default use video mode*/
	mipi_timing->display_mode = MIPI_VIDEO_MODE;
	data = of_get_property(np, "display_mode", NULL);
	if (data) {
		if (!strcmp(data, "video_mode"))
			mipi_timing->display_mode = MIPI_VIDEO_MODE;
		else if (!strcmp(data, "command_mode"))
			mipi_timing->display_mode = MIPI_COMMAND_MODE;
	}

	mipi_timing->auto_stop_clklane_en = of_property_read_bool(np,
							"auto_stop_clklane_en");
	mipi_timing->im_pin_val = of_property_read_bool(np,
							"im_pin_val");

	rc = of_property_read_u32(np, "color_bits", &temp_val);
	if (rc) {
		pr_err("%s:%d, color_bits not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	mipi_timing->color_mode.color_bits= temp_val;

	mipi_timing->color_mode.is_18bit_loosely = of_property_read_bool(np,
						"is_18bit_loosely");

	/*video mode info*/
	if (mipi_timing->display_mode == MIPI_VIDEO_MODE) {
		of_parse_video_mode(np, &mipi_timing->videomode_info);
	} else if (mipi_timing->display_mode == MIPI_COMMAND_MODE) {
		of_parse_command_mode(np, &mipi_timing->commandmode_info);
	}

	of_parse_phy_timing(np, &mipi_timing->phytime_info);

	of_parse_te_info(np, &mipi_timing->teinfo);

	of_parse_ext_info(np, &mipi_timing->ext_info);

	return 0;
}

static int of_parse_rgb_timing(struct device_node *np,
			struct sf_fb_timing_rgb *rgb_timing)
{
	int ret;

	ret = of_parse_video_mode(np, &rgb_timing->videomode_info);
	if (ret) {
		return -EINVAL;
	}

	return ret;
}


static int of_parse_rd_cmd_info(struct device_node *np,
                          struct sf_fb_id_info *rd_id_info, const char *key)
{
	int blen = 0, len;
	int i, cnt;
	const char *data, *bp;
	struct rd_cmd_hdr *hdr;


	data = of_get_property(np, key, &blen);
	if (!data) {
		pr_err("%s: failed, key=%s\n", __func__, key);
		return -ENOMEM;
	}

	bp = data;
	len = blen;
	cnt = 0;
	while (len >= sizeof(*hdr)) {
		hdr = (struct rd_cmd_hdr *)bp;
		if (hdr->id_count > len) {
			pr_err("%s: rd cmd parse error", __func__);
			return -EINVAL;
		}
		bp += sizeof(*hdr);
		len -= sizeof(*hdr);
		bp += hdr->id_count;
		len -= hdr->id_count;
		cnt++;
	}

	if (len != 0) {
		pr_err("%s: rd cmd parse error!", __func__);
		return -EINVAL;
	}

	rd_id_info->num_id_info = cnt;
	rd_id_info->id_info = kzalloc(cnt * sizeof(struct common_id_info),
						GFP_KERNEL);
	if (!rd_id_info->id_info)
		return -ENOMEM;

	bp = data;
	for (i = 0; i < cnt; i++) {
		hdr = (struct rd_cmd_hdr *)bp;
		bp += sizeof(*hdr);
		rd_id_info->id_info[i].hdr = *hdr;
		memcpy(rd_id_info->id_info[i].id, bp, hdr->id_count);
		bp += hdr->id_count;
	}

	return 0;
}

static int of_parse_wr_cmd(struct device_node *np,
                          struct sf_fb_dev_cmds *dev_cmds, const char *key)
{
	int blen = 0, len;
	int i, cnt;
	unsigned int alloc_bytes = 0;
	const char *data, *bp;
	char *buf;
	struct wr_cmd_hdr *hdr;

	data = of_get_property(np, key, &blen);
	if (!data) {
		pr_err("%s: failed, key=%s\n", __func__, key);
		return -ENOMEM;
	}

	bp = data;
	len = blen;
	cnt = 0;
	while (len >= sizeof(*hdr)) {
		hdr = (struct wr_cmd_hdr *)bp;
		if (hdr->dlen > len) {
			pr_err("%s: wr parse error",
					__func__);
			return -EINVAL;
		}
		bp += sizeof(*hdr);
		len -= sizeof(*hdr);
		bp += hdr->dlen;
		len -= hdr->dlen;
		cnt++;
		alloc_bytes += DSI_CMD_LEN(hdr);
	}

	if (len != 0) {
		pr_err("%s: wr parse error!", __func__);
		return -EINVAL;
	}
	dev_cmds->n_pack = cnt;
	dev_cmds->cmds = kzalloc(_ALIGN_UP(alloc_bytes, 4), GFP_KERNEL);

	if (IS_ERR_OR_NULL(dev_cmds->cmds))
		return -ENOMEM;

	bp = data;
	buf = dev_cmds->cmds;
	for (i = 0; i < cnt; i++) {
		len = 0;
		hdr = (struct wr_cmd_hdr *)bp;
		len += sizeof(*hdr);
		len += hdr->dlen;
		memcpy(buf, bp, len);
		bp += len;
		buf += DSI_CMD_LEN(hdr);
	}

	return 0;
}

static int of_parse_gamma_ce_cmd(struct device_node *np,
                          struct sf_fb_prefer_ce *color_info, const char *key)
{
	int types = 0;

	/*FIX ME, we only support up to 3 types, do not overflow.
	* when add new gamma/ce types, please increase COLOR_TYPE_MAX also
	*/
	color_info->info = kzalloc(COLOR_TYPE_MAX * sizeof(struct prefer_ce_info), GFP_KERNEL);
	if (!color_info->info) {
		pr_err("%s no memory!!\n", __func__);
		return -ENOMEM;
	}

	if (!strcmp(key, "gamma")) {
		if (of_find_property(np, "panel-gamma-warm-command", NULL)) {
			of_parse_wr_cmd(np, &color_info->info[types].cmds, "panel-gamma-warm-command");
			color_info->info[types].type = PREFER_WARM;
			types++;
		}
		if (of_find_property(np, "panel-gamma-nature-command", NULL)) {
			of_parse_wr_cmd(np, &color_info->info[types].cmds, "panel-gamma-nature-command");
			color_info->info[types].type = PREFER_NATURE;
			types++;
		}
		if (of_find_property(np, "panel-gamma-cool-command", NULL)) {
			of_parse_wr_cmd(np, &color_info->info[types].cmds, "panel-gamma-cool-command");
			color_info->info[types].type = PREFER_COOL;
			types++;
		}
	} else if (!strcmp(key, "ce")) {
		if (of_find_property(np, "panel-ce-bright-command", NULL)) {
			of_parse_wr_cmd(np, &color_info->info[types].cmds, "panel-ce-bright-command");
			color_info->info[types].type = CE_BRIGHT;
			types++;
		}
		if (of_find_property(np, "panel-ce-std-command", NULL)) {
			of_parse_wr_cmd(np, &color_info->info[types].cmds, "panel-ce-std-command");
			color_info->info[types].type = CE_STANDARD;
			types++;
		}
		if (of_find_property(np, "panel-ce-vivid-command", NULL)) {
			of_parse_wr_cmd(np, &color_info->info[types].cmds, "panel-ce-vivid-command");
			color_info->info[types].type = CE_VELVIA;
			types++;
		}
	}
	if (types > COLOR_TYPE_MAX) {
		pr_err("%s types overflow %d\n", key, types);
		types = COLOR_TYPE_MAX;
	}
	color_info->types = types;

	pr_debug("%s support %d types\n", key, types);

	return 0;
}

static int of_parse_reset_seq(struct device_node *np,
                          u32 rst_seq[RST_SEQ_LEN], u32 *rst_len,
		const char *name)
{
	int num = 0, i;
	int rc;
	struct property *data;
	u32 tmp[RST_SEQ_LEN];

	*rst_len = 0;
	data = of_find_property(np, name, &num);
	num /= sizeof(u32);
	if (!data || !num || num > RST_SEQ_LEN || num % 2) {
		pr_err("%s:%d, error reading %s, length found = %d\n",
				__func__, __LINE__, name, num);
	} else {
		rc = of_property_read_u32_array(np, name, tmp, num);
		if (rc)
			pr_err("%s:%d, error reading %s, rc = %d\n",
					__func__, __LINE__, name, rc);
		else {
			for (i = 0; i < num; ++i)
				rst_seq[i] = tmp[i];
			*rst_len = num;
		}
	}
	return 0;
}

static int sf_displayer_parse_dt(struct device *dev,
                           struct sf_fb_display_dev *pandev)
{
	int rc;
	struct device_node *np = dev->of_node;
	const char *data;
	u32 temp_val;

	dev_dbg(dev, "dsi panel parse dt\n");

	pandev->name = of_get_property(np, "panel_name", NULL);
	pr_info("panel_name: %s\n", pandev->name);

	pandev->interface_info = STARFIVEFB_MIPI_IF;
	data = of_get_property(np, "interface_info", NULL);
	if (data) {
		if (!strcmp(data, "mipi_interface"))
			pandev->interface_info = STARFIVEFB_MIPI_IF;
		else if (!strcmp(data, "rgb_interface"))
			pandev->interface_info = STARFIVEFB_RGB_IF;
	}
	pandev->send_suspend_cmd_in_hs_mode = of_property_read_bool(np,
		"send_suspend_cmd_in_hs");
	/*must define within video mode*/
	rc = of_property_read_u32(np, "refresh_en", &temp_val);
	if (rc && (rc != -EINVAL)) {
		pr_err("%s:%d, Unable to read refresh_en\n",
						__func__, __LINE__);
		return rc;
	} else if (rc != -EINVAL)
		pandev->refresh_en= temp_val;

	pandev->auto_fps = of_property_read_bool(np, "dyn_fps");

	rc = of_property_read_u32(np, "pixel-clock", &temp_val);
	if (rc) {
		pr_err("%s:%d, pixel-clock not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	pandev->pclk= temp_val;

	rc = of_property_read_u32(np, "panel-width", &temp_val);
	if (rc) {
		pr_err("%s:%d, panel width not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	pandev->xres = temp_val;

	rc = of_property_read_u32(np, "panel-height", &temp_val);
	if (rc) {
		pr_err("%s:%d, panel width not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	pandev->yres = temp_val;

	rc = of_property_read_u32(np, "physical-width", &temp_val);
	if (rc && (rc != -EINVAL)) {
		pr_err("%s:%d, Unable to read physical-width\n",
						__func__, __LINE__);
		return rc;
	} else if (rc != -EINVAL)
		pandev->width = temp_val;

	rc = of_property_read_u32(np, "physical-height", &temp_val);
	if (rc && (rc != -EINVAL)) {
		pr_err("%s:%d, Unable to read physical-height\n",
						__func__, __LINE__);
		return rc;
	} else if (rc != -EINVAL)
		pandev->height = temp_val;

	rc = of_property_read_u32(np, "bits-per-pixel", &temp_val);
	if (rc) {
		pr_err("%s:%d, bpp not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	pandev->bpp = temp_val;

	pandev->flags = 0;
	if (of_property_read_bool(np, "gamma-command-monolithic"))
		pandev->flags |= PREFER_CMD_SEND_MONOLITHIC;
	if (of_property_read_bool(np, "ce-command-monolithic"))
		pandev->flags |= CE_CMD_SEND_MONOLITHIC;
	if (of_property_read_bool(np, "resume-with-gamma"))
		pandev->flags |= RESUME_WITH_PREFER;
	if (of_property_read_bool(np, "resume-with-ce"))
		pandev->flags |= RESUME_WITH_CE;

	pandev->init_last = of_property_read_bool(np, "init_last");

	if(STARFIVEFB_MIPI_IF == pandev->interface_info)
		of_parse_mipi_timing(np, &pandev->timing.mipi);	/*mipi info parse*/
	else if (STARFIVEFB_RGB_IF == pandev->interface_info)
		of_parse_rgb_timing(np, &pandev->timing.rgb);

	of_parse_rd_cmd_info(np, &pandev->panel_id_info, "id_read_cmd_info");
	if (of_find_property(np, "pre_id_cmd", NULL))
		of_parse_wr_cmd(np, &pandev->panel_id_info.prepare_cmd, "pre_id_cmd");
	of_parse_rd_cmd_info(np, &pandev->esd_id_info, "esd_read_cmd_info");

	if (of_find_property(np, "pre_esd_cmd", NULL))
		of_parse_wr_cmd(np, &pandev->esd_id_info.prepare_cmd, "pre_esd_cmd");

	of_parse_wr_cmd(np, &pandev->cmds_init, "panel-on-command");
	of_parse_wr_cmd(np, &pandev->cmds_suspend, "panel-off-command");

	of_parse_reset_seq(np, pandev->rst_seq, &pandev->rst_seq_len, "reset-sequence");

	of_parse_gamma_ce_cmd(np, &pandev->display_prefer_info, "gamma");
	of_parse_gamma_ce_cmd(np, &pandev->display_ce_info, "ce");

	dump_panel_info(dev, pandev);

	return 0;
}

static int sf_displayer_probe(struct platform_device *pdev)
{
	struct sf_fb_display_dev *display_dev;
	int ret;

	if (pdev->dev.of_node) {
		display_dev = devm_kzalloc(&pdev->dev,
				sizeof(struct sf_fb_display_dev), GFP_KERNEL);
		if (!display_dev) {
			dev_err(&pdev->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}
		ret = sf_displayer_parse_dt(&pdev->dev, display_dev);
		if (ret) {
			dev_err(&pdev->dev, "DT parsing failed\n");
			return -ENODEV;
		}
	} else {
		dev_err(&pdev->dev, "error: null panel device-tree node");
		return -ENODEV;
	}

	display_dev->power = sf_displayer_power_on;
	display_dev->reset = sf_displayer_reset;
	display_dev->suspend = sf_displayer_suspend;
	display_dev->resume = sf_displayer_resume;

	sf_fb_display_dev_register(display_dev);

	return 0;

}

static int __exit sf_displayer_remove(struct platform_device *dev)
{
	return 0;
}

static struct of_device_id sf_displayer_dt_match[] = {
	{
		.compatible = "starfive,display-dev",
	},
	{}
};

static struct platform_driver sf_displayer_driver = {
	.probe = sf_displayer_probe,
	.remove = __exit_p(sf_displayer_remove),
	.driver = {
		.name = "starfive,display-dev",
		.owner = THIS_MODULE,
		.of_match_table = sf_displayer_dt_match,
	},
};

static int __init sf_displayer_init(void)
{
	return platform_driver_register(&sf_displayer_driver);
}

static void __exit sf_displayer_exit(void)
{
	platform_driver_unregister(&sf_displayer_driver);
}

subsys_initcall(sf_displayer_init);
module_exit(sf_displayer_exit);
MODULE_AUTHOR("StarFive Technology Co., Ltd.");
MODULE_DESCRIPTION("DISPLAYER DRIVER");
MODULE_LICENSE("GPL");
