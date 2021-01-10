/*
 * StarFive isp driver
 *
 * Copyright 2020 StarFive Inc.
 *
 * Licensed under the GPL-2.
 */

#ifndef __SF_ISP_H__
#define __SF_ISP_H__

extern void isp_clk_set(struct stf_vin_dev *vin);
extern void isp_ddr_config(struct stf_vin_dev *vin);
extern void isp_config(struct stf_vin_dev *vin,int isp_id); 

#endif
