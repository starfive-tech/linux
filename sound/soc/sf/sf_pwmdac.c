/**
  ******************************************************************************
  * @file  sf_pwmdac.c
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

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/dmaengine_pcm.h>
#include <sound/sf_pwmdac.h>
#include "pwmdac.h"
#include <linux/kthread.h>

static inline void pwmdc_write_reg(void __iomem *io_base, int reg, u32 val)
{
	writel(val, io_base + reg);
}

static inline u32 pwmdc_read_reg(void __iomem *io_base, int reg)
{
	return readl(io_base + reg);
}

/*
 * 32bit-4byte
*/
static void pwmdac_set_ctrl_enable(struct sf_pwmdac_dev *dev)
{
	u32 date;
	date = pwmdc_read_reg(dev->pwmdac_base, PWMDAC_CTRL);
	pwmdc_write_reg(dev->pwmdac_base, PWMDAC_CTRL, date|0x01 );
}

/*
 * 8:8-bit
 * 10:10-bit
*/
static void pwmdac_set_ctrl_shift(struct sf_pwmdac_dev *dev, u8 data)
{
	u32 value = 0;
 	
	if(data == 8){
		value = (~((~value)|0x02));
		pwmdc_write_reg(dev->pwmdac_base , PWMDAC_CTRL, value);
	}
	else if(data == 10){
		value |= 0x02;
		pwmdc_write_reg(dev->pwmdac_base , PWMDAC_CTRL, value);
	}
}

/*
 * 00:left
 * 01:right
 * 10:center
*/
static void pwmdac_set_ctrl_dutyCycle(struct sf_pwmdac_dev *dev, u8 data)
{
	u32 value = 0;
	
	value = pwmdc_read_reg(dev->pwmdac_base , PWMDAC_CTRL);
	if(data == 0){ //left
		value = (~((~value)|(0x03<<2)));
		pwmdc_write_reg(dev->pwmdac_base , PWMDAC_CTRL, value);
	}
	else if(data == 1){ //right
		value = (~((~value)|(0x01<<3))) | (0x01<<2);
		pwmdc_write_reg(dev->pwmdac_base , PWMDAC_CTRL, value);
	}
	else if(data == 2){ //center
		value = (~((~value)|(0x01<<2))) | (0x01<<3);
		pwmdc_write_reg(dev->pwmdac_base , PWMDAC_CTRL, value);
	}
}


static void pwmdac_set_ctrl_N(struct sf_pwmdac_dev *dev, u16 data)
{
	u32 value = 0;
	
 	value = pwmdc_read_reg(dev->pwmdac_base , PWMDAC_CTRL);
 	pwmdc_write_reg(dev->pwmdac_base , PWMDAC_CTRL, (value & 0xF) | ((data - 1)<<4));
}


static void pwmdac_LR_data_change(struct sf_pwmdac_dev *dev, u8 data)
{
	u32 value = 0;
	
	value = pwmdc_read_reg(dev->pwmdac_base , PWMDAC_CTRL);
	switch(data)
	{
		case NO_CHANGE:
			value &= (~SFC_PWMDAC_LEFT_RIGHT_DATA_CHANGE);
			break;
		case CHANGE:
			value |= SFC_PWMDAC_LEFT_RIGHT_DATA_CHANGE;
			break;		
	}	
	pwmdc_write_reg(dev->pwmdac_base, PWMDAC_CTRL, value);
}


static void pwmdac_data_mode(struct sf_pwmdac_dev *dev,  u8 data)
{
	u32 value = 0;
	
	value = pwmdc_read_reg(dev->pwmdac_base , PWMDAC_CTRL);
	if(data == UNSINGED_DATA){
		value &= (~SFC_PWMDAC_DATA_MODE);
	}
	else if(data == INVERTER_DATA_MSB){
		value |= SFC_PWMDAC_DATA_MODE;
	}
	pwmdc_write_reg(dev->pwmdac_base,PWMDAC_CTRL, value);
}


static int pwmdac_data_shift(struct sf_pwmdac_dev *dev,u8 data)
{    
	u32 value = 0;
	
	if((data < PWMDAC_DATA_LEFT_SHIFT_BIT_0)||(data>PWMDAC_DATA_LEFT_SHIFT_BIT_7)){
		return -1;
	}
	
	value = pwmdc_read_reg(dev->pwmdac_base , PWMDAC_CTRL);
	value &= (~(PWMDAC_DATA_LEFT_SHIFT_BIT_ALL<<15));
	value |= (data<<15);
 	pwmdc_write_reg(dev->pwmdac_base , PWMDAC_CTRL, value);
    return 0;
}

static int get_pwmdac_fifo_state(struct sf_pwmdac_dev *dev)
{
	u32 value;    

	value = pwmdc_read_reg(dev->pwmdac_base , PWMDAC_SATAE);
	if((value & 0x02) == 0)
		return FIFO_UN_FULL;
	
	return FIFO_FULL;
}


static void pwmdac_set(struct sf_pwmdac_dev *dev)
{
	///8-bit + left + N=16
    pwmdac_set_ctrl_shift(dev, dev->shift_bit);
    pwmdac_set_ctrl_dutyCycle(dev, dev->duty_cycle);
    pwmdac_set_ctrl_N(dev, dev->datan);
    pwmdac_set_ctrl_enable(dev);

    pwmdac_LR_data_change(dev, NO_CHANGE);
    pwmdac_data_mode(dev, dev->data_mode);
	if(dev->shift)
	{
		pwmdac_data_shift(dev, dev->shift); 		
	}
}

static int pwmdac_config(struct sf_pwmdac_dev *dev)
{
    switch(dev->mode){
        case shift_8Bit_unsigned:
        case shift_8Bit_unsigned_dataShift:
            /* 8 bit, unsigned */ 
			dev->shift_bit	= PWMDAC_SHIFT_8;
			dev->duty_cycle	= PWMDAC_CYCLE_CENTER;
			dev->datan		= PWMDAC_SAMPLE_CNT_8;
			dev->data_mode	= UNSINGED_DATA;
            break;
			
        case shift_8Bit_inverter:
        case shift_8Bit_inverter_dataShift:
            /* 8 bit, invert */
			dev->shift_bit	= PWMDAC_SHIFT_8;
			dev->duty_cycle	= PWMDAC_CYCLE_CENTER;
			dev->datan		= PWMDAC_SAMPLE_CNT_8;
			dev->data_mode	= INVERTER_DATA_MSB;
            break;
			
        case shift_10Bit_unsigned:
        case shift_10Bit_unsigned_dataShift:
            /* 10 bit, unsigend */
			dev->shift_bit	= PWMDAC_SHIFT_10;
			dev->duty_cycle	= PWMDAC_CYCLE_CENTER;
			dev->datan		= PWMDAC_SAMPLE_CNT_8;
			dev->data_mode	= UNSINGED_DATA;
            break;
			
        case shift_10Bit_inverter:
        case shift_10Bit_inverter_dataShift:
            /* 10 bit, invert */
			dev->shift_bit	= PWMDAC_SHIFT_10;
			dev->duty_cycle	= PWMDAC_CYCLE_CENTER;
			dev->datan		= PWMDAC_SAMPLE_CNT_8;
			dev->data_mode	= INVERTER_DATA_MSB;
            break;
		
        default:
            return -1;
    }	

    if((dev->mode == shift_8Bit_unsigned_dataShift) || (dev->mode == shift_8Bit_inverter_dataShift)
        || (dev->mode == shift_10Bit_unsigned_dataShift) || (dev->mode == shift_10Bit_inverter_dataShift))
    {
        dev->shift = 4; /*0~7*/
    }else{
        dev->shift = 0;
    }	
    return 0;
}


static int sf_pwmdac_prepare(struct snd_pcm_substream *substream,
			  struct snd_soc_dai *dai)
{	
	struct sf_pwmdac_dev *dev = snd_soc_dai_get_drvdata(dai);
	pwmdac_set(dev);
	return 0;
}

static int pwmdac_tx_thread(void *dev)
{
	struct sf_pwmdac_dev *pwmdac_dev =  (struct sf_pwmdac_dev *)dev;

	set_current_state(TASK_INTERRUPTIBLE);
	while (!schedule_timeout(usecs_to_jiffies(50))) {
		if(pwmdac_dev->tx_thread_exit)
			break;
		if(get_pwmdac_fifo_state(pwmdac_dev)==0){
			sf_pwmdac_pcm_push_tx(pwmdac_dev);
		}
		
		set_current_state(TASK_INTERRUPTIBLE);
	}
	
	pwmdac_dev->tx_thread = NULL;
	return 0;
}

static int sf_pwmdac_trigger(struct snd_pcm_substream *substream,
		int cmd, struct snd_soc_dai *dai)
{
	struct sf_pwmdac_dev *dev = snd_soc_dai_get_drvdata(dai);
	int ret = 0;
	
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		dev->active++;
		if (dev->use_pio) {
			dev->tx_thread = kthread_create(pwmdac_tx_thread, (void *)dev, "pwmdac");
			if (IS_ERR(dev->tx_thread)) {
				return PTR_ERR(dev->tx_thread);
			}
			wake_up_process(dev->tx_thread);
			dev->tx_thread_exit = 0;
		}
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		dev->active--;
		if (dev->use_pio) {
			  if(dev->tx_thread){  
				dev->tx_thread_exit = 1;
            }
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;

	return 0;
}


static const struct snd_soc_dai_ops sf_pwmdac_dai_ops = {
	.prepare	= sf_pwmdac_prepare,
	.trigger	= sf_pwmdac_trigger,
};

static const struct snd_soc_component_driver sf_pwmdac_component = {
	.name		= "sf-pwmdac",
};

static struct snd_soc_dai_driver pwmdac_dai = {
	.name = "pwmdac",
	.id = 0,
	.playback = {
		.channels_min = 1,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_U8 |SNDRV_PCM_FMTBIT_S16_LE,
	},
	.ops = &sf_pwmdac_dai_ops,
};

static int sf_pwmdac_probe(struct platform_device *pdev)
{
	const struct pwmdac_platform_data *pdata = pdev->dev.platform_data;
	struct sf_pwmdac_dev *dev;
	struct resource *res;
	int ret;
	
 	dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	dev->pwmdac_base  = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(dev->pwmdac_base))
		return PTR_ERR(dev->pwmdac_base);
	
	dev->dev = &pdev->dev;
	dev->mode = shift_8Bit_unsigned;
	dev->fifo_th = 2;//8byte
	pwmdac_config(dev);

	dev->use_pio = true;

	//todo dma config 
	//dev->use_pio = false;
	
	dev_set_drvdata(&pdev->dev, dev);
	ret = devm_snd_soc_register_component(&pdev->dev, &sf_pwmdac_component,
					 &pwmdac_dai, 1);
	if (ret != 0) {
		dev_err(&pdev->dev, "not able to register dai\n");
		return ret;
	}

	if (dev->use_pio) {
		ret = sf_pwmdac_pcm_register(pdev);
	} else {
		ret = devm_snd_dmaengine_pcm_register(&pdev->dev, NULL,
				0);
	}
  	return 0;
}


static int sf_pwmdac_remove(struct platform_device *pdev)
{
  	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id sf_pwmdac_of_match[] = {
	{ .compatible = "sf,pwmdac",	 },
	{},
};

MODULE_DEVICE_TABLE(of, sf_pwmdac_of_match);
#endif


static struct platform_driver sf_pwmdac_driver = {
	.probe		= sf_pwmdac_probe,
	.remove		= sf_pwmdac_remove,
	.driver		= {
		.name	= "sf-pwmdac",
		.of_match_table = of_match_ptr(sf_pwmdac_of_match),
	},
};

module_platform_driver(sf_pwmdac_driver);

MODULE_AUTHOR("jenny.zhang <jenny.zhang@starfivetech.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("startfive pwmdac SoC Interface");
MODULE_ALIAS("platform:startfive-pwmdac");
