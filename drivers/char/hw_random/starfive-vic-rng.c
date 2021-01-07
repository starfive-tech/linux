/*
 ******************************************************************************
 * @file  starfive-vic-rng.c
 * @author  StarFive Technology
 * @version  V1.0
 * @date  08/13/2020
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
 * COPYRIGHT 2020 Shanghai StarFive Technology Co., Ltd.
 */
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/hw_random.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/random.h>

#include "starfive-vic-rng.h"

#define to_vic_rng(p)	container_of(p, struct vic_rng, rng)

struct vic_rng {
	struct device	*dev;
	void __iomem	*base;
	struct hwrng	rng;
};

static inline void vic_wait_till_idle(struct vic_rng *hrng)
{
	while(readl(hrng->base + VIC_STAT) & VIC_STAT_BUSY)
		;
}

static inline void vic_rng_irq_mask_clear(struct vic_rng *hrng)
{
	// clear register: ISTAT
	u32 data = readl(hrng->base + VIC_ISTAT);
	writel(data, hrng->base + VIC_ISTAT);
	writel(0, hrng->base + VIC_ALARM);
}

static int vic_trng_cmd(struct vic_rng *hrng, u32 cmd) {
	int res = 0;
	// wait till idle
	vic_wait_till_idle(hrng);
	switch (cmd) {
	case VIC_CTRL_CMD_NOP:
	case VIC_CTRL_CMD_GEN_NOISE:
	case VIC_CTRL_CMD_GEN_NONCE:
	case VIC_CTRL_CMD_CREATE_STATE:
	case VIC_CTRL_CMD_RENEW_STATE:
	case VIC_CTRL_CMD_REFRESH_ADDIN:
	case VIC_CTRL_CMD_GEN_RANDOM:
	case VIC_CTRL_CMD_ADVANCE_STATE:
	case VIC_CTRL_CMD_KAT:
	case VIC_CTRL_CMD_ZEROIZE:
		writel(cmd, hrng->base + VIC_CTRL);
		break;
	default:
		res = -1;
		break;
	}

	return res;
}

static int vic_rng_init(struct hwrng *rng)
{
	struct vic_rng *hrng = to_vic_rng(rng);

	// wait till idle

	// clear register: ISTAT
	vic_rng_irq_mask_clear(hrng);

	// set mission mode
	writel(VIC_SMODE_SECURE_EN(1), hrng->base + VIC_SMODE);

	vic_trng_cmd(hrng, VIC_CTRL_CMD_GEN_NOISE);
	vic_wait_till_idle(hrng);

	// set interrupt
	writel(VIC_IE_ALL, hrng->base + VIC_IE);

	// zeroize
	vic_trng_cmd(hrng, VIC_CTRL_CMD_ZEROIZE);

	vic_wait_till_idle(hrng);

	return 0;
}

static irqreturn_t vic_rng_irq(int irq, void *priv)
{
	u32 status, val;
	struct vic_rng *hrng = (struct vic_rng *)priv;

	/*
	 * clearing the interrupt will also clear the error register
	 * read error and status before clearing
	 */
	status = readl(hrng->base + VIC_ISTAT);

	if (status & VIC_ISTAT_ALARMS) {
		writel(VIC_ISTAT_ALARMS, hrng->base + VIC_ISTAT);
		val = readl(hrng->base + VIC_ALARM);
		if (val & VIC_ALARM_ILLEGAL_CMD_SEQ) {
			writel(VIC_ALARM_ILLEGAL_CMD_SEQ, hrng->base + VIC_ALARM);
			//dev_info(hrng->dev, "ILLEGAL CMD SEQ: LAST_CMD=0x%x\r\n",
			//VIC_STAT_LAST_CMD(readl(hrng->base + VIC_STAT)));
		} else {
			dev_info(hrng->dev, "Failed test: %x\r\n", val);
		}
	}

	if (status & VIC_ISTAT_ZEROIZE) {
		writel(VIC_ISTAT_ZEROIZE, hrng->base + VIC_ISTAT);
		//dev_info(hrng->dev, "zeroized\r\n");
	}

	if (status & VIC_ISTAT_KAT_COMPLETE) {
		writel(VIC_ISTAT_KAT_COMPLETE, hrng->base + VIC_ISTAT);
		//dev_info(hrng->dev, "kat_completed\r\n");
	}

	if (status & VIC_ISTAT_NOISE_RDY) {
		writel(VIC_ISTAT_NOISE_RDY, hrng->base + VIC_ISTAT);
		//dev_info(hrng->dev, "noise_rdy\r\n");
	}

	if (status & VIC_ISTAT_DONE) {
		writel(VIC_ISTAT_DONE, hrng->base + VIC_ISTAT);
		//dev_info(hrng->dev, "done\r\n");
		/*
		if (VIC_STAT_LAST_CMD(readl(hrng->base + VIC_STAT)) ==
		    VIC_CTRL_CMD_GEN_RANDOM) {
			dev_info(hrng->dev, "Need Update Buffer\r\n");
		}
		*/
	}
	vic_rng_irq_mask_clear(hrng);

	return IRQ_HANDLED;
}

static void vic_rng_cleanup(struct hwrng *rng)
{
	struct vic_rng *hrng = to_vic_rng(rng);

	writel(0, hrng->base + VIC_CTRL);
}

static int vic_rng_read(struct hwrng *rng, void *buf, size_t max, bool wait)
{
	struct vic_rng *hrng = to_vic_rng(rng);

	vic_trng_cmd(hrng, VIC_CTRL_CMD_ZEROIZE);
	vic_trng_cmd(hrng, VIC_CTRL_CMD_GEN_NOISE);
	vic_trng_cmd(hrng, VIC_CTRL_CMD_CREATE_STATE);

	vic_wait_till_idle(hrng);
	max = min_t(size_t, max, (VIC_RAND_LEN * 4));

	writel(0x0, hrng->base + VIC_MODE);
	vic_trng_cmd(hrng, VIC_CTRL_CMD_GEN_RANDOM);

	vic_wait_till_idle(hrng);
	memcpy_fromio(buf, hrng->base + VIC_RAND0, max);
	vic_trng_cmd(hrng, VIC_CTRL_CMD_ZEROIZE);

	vic_wait_till_idle(hrng);
	return max;
}

static int vic_rng_probe(struct platform_device *pdev)
{
	int ret;
	int irq;
	struct vic_rng *rng;
	struct resource *res;

	rng = devm_kzalloc(&pdev->dev, sizeof(*rng), GFP_KERNEL);
	if (!rng){
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, rng);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	rng->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(rng->base)){
		return PTR_ERR(rng->base);
	}

	irq = platform_get_irq(pdev, 0);
	if (irq <= 0) {
		dev_err(&pdev->dev, "Couldn't get irq %d\n", irq);
		return irq;
	}

	ret = devm_request_irq(&pdev->dev, irq, vic_rng_irq, 0, pdev->name,
				(void *)rng);
	if (ret) {
		dev_err(&pdev->dev, "Can't get interrupt working.\n");
		return ret;
	}

	rng->rng.name = pdev->name;
	rng->rng.init = vic_rng_init;
	rng->rng.cleanup = vic_rng_cleanup;
	rng->rng.read = vic_rng_read;

	rng->dev = &pdev->dev;

	ret = devm_hwrng_register(&pdev->dev, &rng->rng);
	if (ret) {
		dev_err(&pdev->dev, "failed to register hwrng\n");
		return ret;
	}

	dev_info(&pdev->dev, "Initialized\n");

	return 0;
}

static const struct of_device_id vic_rng_dt_ids[] = {
	{ .compatible = "starfive,vic-rng" },
	{ }
};
MODULE_DEVICE_TABLE(of, vic_rng_dt_ids);

static struct platform_driver vic_rng_driver = {
	.probe		= vic_rng_probe,
	.driver		= {
		.name		= "vic-rng",
		.of_match_table	= of_match_ptr(vic_rng_dt_ids),
	},
};

module_platform_driver(vic_rng_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Huan Feng <huan.feng@starfivetech.com>");
MODULE_DESCRIPTION("Starfive VIC random number generator driver");
