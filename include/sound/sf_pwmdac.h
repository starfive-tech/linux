/**
  ******************************************************************************
  * @file  sf_pwmdac.h
  * @author  StarFive Technology
  * @version  V1.0
  * @date  05/27/2021
  * @brief
  ******************************************************************************
  * @copy
  *
  * THE PRESENT SOFTWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, STARFIVE SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH SOFTWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT 20120 Shanghai StarFive Technology Co., Ltd. </center></h2>
  */

#ifndef __SOUND_STARTFIVE_PWMDAC_H
#define __SOUND_STARTFIVE_PWMDAC_H

#include <linux/dmaengine.h>
#include <linux/types.h>

struct pwmdac_platform_data {
	#define SF_PWMDAC_PLAY	(1 << 0)
	u32 snd_fmts;
	u32 snd_rates;
	unsigned int quirks;

	void *play_dma_data;
	bool (*filter)(struct dma_chan *chan, void *slave);
};

struct pwmdac_dma_data {
	void *data;
	dma_addr_t addr;
	u32 max_burst;
	enum dma_slave_buswidth addr_width;
	bool (*filter)(struct dma_chan *chan, void *slave);
};


#endif /*  __SOUND_STARTFIVE_PWMDAC_H */
