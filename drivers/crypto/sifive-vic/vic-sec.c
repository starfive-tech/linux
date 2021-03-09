/*
 ******************************************************************************
 * @file  vic-sec.c
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
 * <h2><center>&copy; COPYRIGHT 2020 Shanghai StarFive Technology Co., Ltd. </center></h2>
 */
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>

#include <crypto/aes.h>
#include <crypto/des.h>
#include <crypto/scatterwalk.h>
#include <crypto/internal/aead.h>

#include "vic-sec.h"

#define DRIVER_NAME             "vic-sec"

#define CRYP_AUTOSUSPEND_DELAY	50

struct vic_dev_list {
	struct list_head        dev_list;
	spinlock_t              lock; /* protect dev_list */
};

static struct vic_dev_list dev_list = {
	.dev_list = LIST_HEAD_INIT(dev_list.dev_list),
	.lock     = __SPIN_LOCK_UNLOCKED(dev_list.lock),
};

struct vic_sec_dev *vic_sec_find_dev(struct vic_sec_ctx *ctx)
{
	struct vic_sec_dev *sdev = NULL, *tmp;

	spin_lock_bh(&dev_list.lock);
	if (!ctx->sdev) {
		list_for_each_entry(tmp, &dev_list.dev_list, list) {
			sdev = tmp;
			break;
		}
		ctx->sdev = sdev;
	} else {
		sdev = ctx->sdev;
	}

	spin_unlock_bh(&dev_list.lock);

	return sdev;
}

static irqreturn_t vic_cryp_irq_thread(int irq, void *arg)
{
	struct vic_sec_dev *sdev = (struct vic_sec_dev *) arg;

	mutex_unlock(&sdev->doing);

	return IRQ_HANDLED;
}

static irqreturn_t vic_cryp_irq(int irq, void *arg)
{
	struct vic_sec_dev *sdev = (struct vic_sec_dev *) arg;
	irqreturn_t ret = IRQ_WAKE_THREAD;

	if(sdev->status.aes_busy || sdev->status.sha_busy) {
		sdev->status.v = readl(sdev->io_base + SEC_STATUS_REG);
		writel(sdev->status.v, sdev->io_base + SEC_STATUS_REG);
	} else {
		ret = vic_pka_irq_done(sdev);
	}

	return ret;
}
static const struct of_device_id vic_dt_ids[] = {
	{ .compatible = "starfive,vic-sec", .data = NULL},
	{},
};
MODULE_DEVICE_TABLE(of, vic_dt_ids);

extern void vic_hash_test(struct vic_sec_dev *sdev);

static int vic_cryp_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct vic_sec_dev *sdev;
	struct resource *res;
	int irq, ret;
	int pages = 0;

	sdev = devm_kzalloc(dev, sizeof(*sdev), GFP_KERNEL);
	if (!sdev)
		return -ENOMEM;

	sdev->dev = dev;

	mutex_init(&sdev->lock);
	mutex_init(&sdev->doing);

	sdev->io_base = devm_platform_ioremap_resource_byname(pdev, "secmem");
	if (IS_ERR(sdev->io_base))
		return PTR_ERR(sdev->io_base);

	sdev->clk_base = devm_platform_ioremap_resource_byname(pdev, "secclk");
	if (IS_ERR(sdev->clk_base))
		return PTR_ERR(sdev->clk_base);

	sdev->pka.regbase = sdev->io_base + PKA_IO_BASE_OFFSET;

	/* pka irq handle check */
	sdev->status.v = 0;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(dev, "Cannot get IRQ resource\n");
		return irq;
	}

	ret = devm_request_threaded_irq(dev, irq, vic_cryp_irq,
					vic_cryp_irq_thread, IRQF_ONESHOT,
					dev_name(dev), sdev);
	if (ret) {
		dev_err(&pdev->dev, "Can't get interrupt working.\n");
		return ret;
	}

	sdev->clk = devm_clk_get(dev, NULL);
	if (IS_ERR(sdev->clk)) {
		dev_err(dev, "Could not get clock\n");
		return PTR_ERR(sdev->clk);
	}

	ret = clk_prepare_enable(sdev->clk);
	if (ret) {
		dev_err(sdev->dev, "Failed to enable clock\n");
		return ret;
	}

	platform_set_drvdata(pdev, sdev);

	spin_lock(&dev_list.lock);
	list_add(&sdev->list, &dev_list.dev_list);
	spin_unlock(&dev_list.lock);

	pages = get_order(VIC_AES_MSG_RAM_SIZE);

	sdev->data = (void *)__get_free_pages(GFP_KERNEL, pages);
	if (!sdev->data) {
		dev_err(sdev->dev, "Can't allocate pages when unaligned\n");
		return -EFAULT;
	}
	sdev->data_buf_len = VIC_AES_BUF_RAM_SIZE;
	sdev->pages_count = pages;

	/* Initialize crypto engine */
	sdev->engine = crypto_engine_alloc_init(dev, 1);
	if (!sdev->engine) {
		ret = -ENOMEM;
		goto err_engine;
	}

	ret = crypto_engine_start(sdev->engine);
	if (ret)
		goto err_engine_start;

	ret = vic_hash_register_algs();
	if (ret) {
		goto err_algs_sha;
	}

	vic_clk_enable(sdev,AES_CLK);
	ret = vic_aes_register_algs();
	if (ret) {
		vic_clk_disable(sdev,AES_CLK);
		dev_err(dev, "Could not register algs\n");
		goto err_algs_aes;
	}
	vic_clk_disable(sdev,AES_CLK);

	vic_clk_enable(sdev,PKA_CLK);
	ret = vic_pka_init(&sdev->pka);
	if (ret) {
		vic_clk_disable(sdev,PKA_CLK);
		dev_err(dev, "pka init error\n");
		goto err_pka_init;
	}

	ret = vic_pka_register_algs();
	if (ret) {
		vic_clk_disable(sdev,PKA_CLK);
		dev_err(dev, "Could not register algs\n");
		goto err_algs_pka;
	}
	vic_clk_disable(sdev,PKA_CLK);
	dev_info(dev, "Initialized\n");

	return 0;

err_algs_pka:
err_pka_init:
	vic_aes_unregister_algs();
err_algs_aes:
	vic_hash_unregister_algs();
err_engine_start:
	crypto_engine_exit(sdev->engine);
err_engine:
err_algs_sha:
	free_pages((unsigned long)sdev->data, pages);
	spin_lock(&dev_list.lock);
	list_del(&sdev->list);
	spin_unlock(&dev_list.lock);

	clk_disable_unprepare(sdev->clk);

	return ret;
}

static int vic_cryp_remove(struct platform_device *pdev)
{
	struct vic_sec_dev *sdev = platform_get_drvdata(pdev);

	if (!sdev)
		return -ENODEV;


	vic_pka_unregister_algs();
	vic_aes_unregister_algs();
	vic_hash_unregister_algs();

	crypto_engine_exit(sdev->engine);

	free_pages((unsigned long)sdev->data, sdev->pages_count);

	spin_lock(&dev_list.lock);
	list_del(&sdev->list);
	spin_unlock(&dev_list.lock);

	clk_disable_unprepare(sdev->clk);

	return 0;
}

#ifdef CONFIG_PM
static int vic_cryp_runtime_suspend(struct device *dev)
{
	struct vic_sec_dev *cryp = dev_get_drvdata(dev);

	clk_disable_unprepare(cryp->clk);

	return 0;
}

static int vic_cryp_runtime_resume(struct device *dev)
{
	struct vic_sec_dev *cryp = dev_get_drvdata(dev);
	int ret;

	ret = clk_prepare_enable(cryp->clk);
	if (ret) {
		dev_err(cryp->dev, "Failed to prepare_enable clock\n");
		return ret;
	}

	return 0;
}
#endif

static const struct dev_pm_ops vic_cryp_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				pm_runtime_force_resume)
	SET_RUNTIME_PM_OPS(vic_cryp_runtime_suspend,
			   vic_cryp_runtime_resume, NULL)
};

static struct platform_driver vic_cryp_driver = {
	.probe  = vic_cryp_probe,
	.remove = vic_cryp_remove,
	.driver = {
		.name           = DRIVER_NAME,
		.pm		= &vic_cryp_pm_ops,
		.of_match_table = vic_dt_ids,
	},
};

module_platform_driver(vic_cryp_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Huan Feng <huan.feng@starfivetech.com>");
MODULE_DESCRIPTION("Starfive VIC CRYP SHA and AES driver");
