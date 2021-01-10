/*
 * StarFive isp driver
 *
 * Copyright 2020 StarFive Inc.
 *
 * Licensed under the GPL-2.
 */

#ifndef __SF_CSI_H__
#define __SF_CSI_H__

union static_config{
    u32 raw;
    struct {
        u32 sel    : 2;
        u32 rsvd_6 : 2;
        u32 v2p0_support_enable : 1;
        u32 rsvd_5 : 3;
        
        u32 lane_nb : 3;
        u32 rsvd_4 : 5;

        u32 dl0_map : 3;
        u32 rsvd_3 : 1;
        u32 dl1_map : 3;
        u32 rsvd_2 : 1;

        u32 dl2_map : 3;
        u32 rsvd_1 : 1;
        u32 dl3_map : 3;
        u32 rsvd_0 : 1;
    } bits;
};

union error_bypass_cfg {
    u32 value;
    struct {
        u32 crc             :  1;
        u32 ecc             :  1;
        u32 data_id         :  1;
        u32 rsvd_0          : 29;
    };
};

union stream_monitor_ctrl {
    u32 value;
    struct {
        u32 lb_vc             : 4;
        u32 lb_en             : 1;
        u32 timer_vc          : 4;
        u32 timer_en          : 1;
        u32 timer_eof         : 1;
        u32 frame_mon_vc      : 4;
        u32 frame_mon_en      : 1;
        u32 frame_length      : 16;
    };
};

union stream_cfg {
    u32 value;
    struct {
        u32 interface_mode :  1;
        u32 ls_le_mode     :  1;
        u32 rsvd_3         :  2;
        u32 num_pixels     :  2;
        u32 rsvd_2         :  2;
        u32 fifo_mode      :  2;
        u32 rsvd_1         :  2;
        u32 bpp_bypass     :  3;
        u32 rsvd_0         :  1;
        u32 fifo_fill      : 16;
    };
};

union dphy_lane_ctrl{
    u32 raw;
    struct {
        u32 dl0_en : 1;
        u32 dl1_en : 1;
        u32 dl2_en : 1;
        u32 dl3_en : 1;
        u32 cl_en : 1;
        u32 rsvd_1 : 7;

        u32 dl0_reset : 1;
        u32 dl1_reset : 1;
        u32 dl2_reset : 1;
        u32 dl3_reset : 1;
        u32 cl_reset : 1;
        u32 rsvd_0 : 15;
    } bits;
};

union dphy_lane_swap{
        u32 raw;
        struct {
            u32 rx_1c2c_sel        :1;
            u32 lane_swap_clk      :3;
            u32 lane_swap_clk1     :3;
            u32 lane_swap_lan0     :3;
            u32 lane_swap_lan1     :3;
            u32 lane_swap_lan2     :3;
            u32 lane_swap_lan3     :3;
            u32 dpdn_swap_clk      :1;
            u32 dpdn_swap_clk1     :1;
            u32 dpdn_swap_lan0     :1;
            u32 dpdn_swap_lan1     :1;
            u32 dpdn_swap_lan2     :1;
            u32 dpdn_swap_lan3     :1;
            u32 hs_freq_chang_clk0 :1;
            u32 hs_freq_chang_clk1 :1;
            u32 reserved           :5;
       } bits;
};

union dphy_lane_en{
	u32 raw;
	struct {
		u32 gpio_en			: 6;
		u32 mp_test_mode_sel	: 5;
		u32 mp_test_en 		: 1;
		u32 dphy_enable_lan0	: 1;
		u32 dphy_enable_lan1	: 1;
		u32 dphy_enable_lan2	: 1;
		u32 dphy_enable_lan3	: 1;
		u32 rsvd_0 			: 16; 
	} bits;
};

extern int csi2rx_dphy_config(struct stf_vin_dev *vin,const csi2rx_dphy_cfg_t *cfg);
extern int csi2rx_config(struct stf_vin_dev *vin,int id, const csi2rx_cfg_t *cfg);


#endif

