// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2021 StarFive, Inc <samin.guo@starfivetech.com>
 *
 * THE PRESENT SOFTWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING
 * CUSTOMERS WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER
 * FOR THEM TO SAVE TIME. AS A RESULT, STARFIVE SHALL NOT BE HELD LIABLE
 * FOR ANY DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY
 * CLAIMS ARISING FROM THE CONTENT OF SUCH SOFTWARE AND/OR THE USE MADE
 * BY CUSTOMERS OF THE CODING INFORMATION CONTAINED HEREIN IN CONNECTION
 * WITH THEIR PRODUCTS.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/mfd/syscon.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/timer.h>
#include <linux/uaccess.h>
#include <linux/watchdog.h>

#define WDGINTSTAUS	0x0
#define WDOGCONTROL	0x104	/* Watchdog Control Register R/W */
#define WDOGLOAD	0x108	/* The initial value to be loaded */
				/* into the counter and is also used */
				/* as the reload value. R/W */
#define WDOGEN		0x110	/* Watchdog enable Register */
#define WDOGRELOAD	0x114	/* Write this register to reload preset */
				/* value to counter. (Write 0 or 1 are both ok) */
#define WDOGVALUE	0x118	/* Watchdog Value Register RO */
#define WDOGINTCLR	0x120	/* Watchdog Clear Interrupt Register WO */
#define WDOGINTMSK	0x124	/* Watchdog Interrupt Mask Register */
#define WDOGLOCK	0x13c	/* Watchdog Lock Register  R/W */

/* WDOGCONTROL */
#define WDOG_INT_EN	0x0
#define WDOG_RESET_EN	0x1

/* WDOGLOCK */
#define WDOG_UNLOCKED	(0x378f0765)
#define WDOG_LOCKED	BIT(0)

#define SI5_WATCHDOG_ENABLE	0x1
#define SI5_WATCHDOG_ATBOOT	0x0
#define SI5_WATCHDOG_MAXCNT	0xffffffff

#define SI5_WATCHDOG_DEFAULT_TIME	(15)

static bool nowayout = WATCHDOG_NOWAYOUT;
static int tmr_margin;
static int tmr_atboot = SI5_WATCHDOG_ATBOOT;
static int soft_noboot;

module_param(tmr_margin, int, 0);
module_param(tmr_atboot, int, 0);
module_param(nowayout, bool, 0);
module_param(soft_noboot, int, 0);

MODULE_PARM_DESC(tmr_margin, "Watchdog tmr_margin in seconds. (default="
		__MODULE_STRING(SI5_WATCHDOG_DEFAULT_TIME) ")");
MODULE_PARM_DESC(tmr_atboot,
		"Watchdog is started at boot time if set to 1, default="
			__MODULE_STRING(SI5_WATCHDOG_ATBOOT));
MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started (default="
			__MODULE_STRING(WATCHDOG_NOWAYOUT) ")");
MODULE_PARM_DESC(soft_noboot, "Watchdog action, set to 1 to ignore reboots, 0 to reboot (default 0)");

struct si5_wdt_variant {
	u32 control;
	u32 load;
	u32 enable;
	u32 reload;
	u32 value;
	u32 int_clr;
	u32 int_mask;
	u32 unlock;
};

struct stf_si5_wdt {
	u64 freq;
	struct device *dev;
	struct watchdog_device wdt_device;
	struct clk *clock;
	const struct si5_wdt_variant *drv_data;
	u32 count;	/*count of timeout*/
	u32 reload;	/*restore the count*/
	void __iomem *base;
	spinlock_t lock;
};

#ifdef CONFIG_OF
static const struct si5_wdt_variant drv_data_jh7100 = {
	.control = WDOGCONTROL,
	.load = WDOGLOAD,
	.enable = WDOGEN,
	.reload = WDOGRELOAD,
	.value = WDOGVALUE,
	.int_clr = WDOGINTCLR,
	.int_mask = WDOGINTMSK,
	.unlock = WDOGLOCK,
};

static const struct of_device_id starfive_wdt_match[] = {
	{ .compatible = "starfive,si5-wdt",
	  .data = &drv_data_jh7100 },
	{},
};
MODULE_DEVICE_TABLE(of, starfive_wdt_match);
#endif

static const struct platform_device_id si5wdt_ids[] = {
	{
		.name = "starfive-si5-wdt",
		.driver_data = (unsigned long)&drv_data_jh7100,
	},
	{}
};
MODULE_DEVICE_TABLE(platform, si5wdt_ids);

static u64
si5wdt_get_clock_rate(struct device_node *np)
{
	int ret;
	u32 freq;

	ret = of_property_read_u32(np, "clock-frequency", &freq);
	if (ret)
		pr_warn("get rate failed\n");

	return (u64)freq;
}

static __maybe_unused
u32 si5wdt_sec_to_ticks(struct stf_si5_wdt *wdt, u32 sec)
{
	return sec * wdt->freq;
}

static __maybe_unused
u32 si5wdt_ticks_to_sec(struct stf_si5_wdt *wdt, u32 ticks)
{
	return DIV_ROUND_CLOSEST(ticks, wdt->freq);
}

/*
 * Write 0x378f0765 to unlock. Write other value to lock. When lock bit is 1,
 * external accesses to other watchdog registers are ignored.
 */
static int si5wdt_is_locked(struct stf_si5_wdt *wdt)
{
	u32 val;

	val = readl(wdt->base + wdt->drv_data->unlock);
	return !!(val & WDOG_LOCKED);
}

static void si5wdt_unlock(struct stf_si5_wdt *wdt)
{
	if (si5wdt_is_locked(wdt))
		writel(WDOG_UNLOCKED, wdt->base + wdt->drv_data->unlock);
}

static void si5wdt_lock(struct stf_si5_wdt *wdt)
{
	if (!si5wdt_is_locked(wdt))
		writel(~WDOG_UNLOCKED, wdt->base + wdt->drv_data->unlock);
}

static int __maybe_unused si5wdt_is_running(struct stf_si5_wdt *wdt)
{
	u32 val;

	si5wdt_unlock(wdt);
	val = readl(wdt->base + wdt->drv_data->control);
	si5wdt_lock(wdt);

	return !!(val & SI5_WATCHDOG_ENABLE);
}

static void si5wdt_int_enable(struct stf_si5_wdt *wdt)
{
	u32 val;

	val = readl(wdt->base + wdt->drv_data->int_mask);
	val &= ~(1<<0);
	writel(val, wdt->base + wdt->drv_data->int_mask);
}

static void si5wdt_int_disable(struct stf_si5_wdt *wdt)
{
	u32 val;

	val = readl(wdt->base + wdt->drv_data->int_mask);
	val |= (1<<0);
	writel(val, wdt->base + wdt->drv_data->int_mask);
}

static void si5wdt_enable_reset(struct stf_si5_wdt *wdt)
{
	u32 val;

	val = readl(wdt->base + wdt->drv_data->control);
	val |= WDOG_RESET_EN;
	/* enable wdog interrupt to reset */
	writel(val, wdt->base + wdt->drv_data->control);
}

void si5wdt_disable_reset(struct stf_si5_wdt *wdt)
{
	u32 val;

	val = readl(wdt->base + wdt->drv_data->control);
	val &= ~WDOG_RESET_EN;
	/*disable wdog interrupt to reset*/
	writel(val, wdt->base + wdt->drv_data->control);
}

static void si5wdt_int_clr(struct stf_si5_wdt *wdt)
{
	void __iomem *addr;

	addr = wdt->base + wdt->drv_data->int_clr;

	/* waiting interrupt can be to clearing */
	do {

	} while (readl(addr) & BIT(1));

	writel(0x1, addr);
}

static inline void si5wdt_set_count(struct stf_si5_wdt *wdt, u32 val)
{
	writel(val, wdt->base + wdt->drv_data->load);
}

static inline void
si5wdt_set_relod_count(struct stf_si5_wdt *wdt, u32 count)
{
	writel(count, wdt->base + wdt->drv_data->load);
	writel(0x1, wdt->base + wdt->drv_data->reload);
}

static inline u32 si5wdt_get_count(struct stf_si5_wdt *wdt)
{
	return readl(wdt->base + wdt->drv_data->value);
}

static inline void si5wdt_enable(struct stf_si5_wdt *wdt)
{
	writel(SI5_WATCHDOG_ENABLE, wdt->base + wdt->drv_data->enable);
}

static inline void si5wdt_disable(struct stf_si5_wdt *wdt)
{
	writel(~SI5_WATCHDOG_ENABLE, wdt->base + wdt->drv_data->enable);
}

static int si5wdt_mask_and_disable_reset(struct stf_si5_wdt *wdt, bool mask)
{
	si5wdt_unlock(wdt);

	if (mask)
		si5wdt_disable_reset(wdt);
	else
		si5wdt_enable_reset(wdt);

	si5wdt_lock(wdt);

	return 0;
}

static unsigned int si5wdt_max_timeout(struct stf_si5_wdt *wdt)
{
	return DIV_ROUND_UP(SI5_WATCHDOG_MAXCNT, wdt->freq) - 1;
}

static unsigned int si5wdt_get_timeleft(struct watchdog_device *wdd)
{
	struct stf_si5_wdt *wdt = watchdog_get_drvdata(wdd);
	u32 count;

	si5wdt_unlock(wdt);
	count = si5wdt_get_count(wdt);
	si5wdt_lock(wdt);

	return si5wdt_ticks_to_sec(wdt, count);
}

static int si5wdt_keepalive(struct watchdog_device *wdd)
{
	struct stf_si5_wdt *wdt = watchdog_get_drvdata(wdd);

	spin_lock(&wdt->lock);

	si5wdt_unlock(wdt);
	si5wdt_set_relod_count(wdt, wdt->count);
	si5wdt_lock(wdt);

	spin_unlock(&wdt->lock);

	return 0;
}

static irqreturn_t si5wdt_interrupt_handler(int irq, void *data)
{
	struct stf_si5_wdt *wdt = platform_get_drvdata(data);

	si5wdt_unlock(wdt);
	si5wdt_int_clr(wdt);
	si5wdt_keepalive(&wdt->wdt_device);
	si5wdt_lock(wdt);

	return IRQ_HANDLED;
}

static int si5wdt_stop(struct watchdog_device *wdd)
{
	struct stf_si5_wdt *wdt = watchdog_get_drvdata(wdd);

	spin_lock(&wdt->lock);

	si5wdt_unlock(wdt);
	si5wdt_int_disable(wdt);
	si5wdt_int_clr(wdt);
	si5wdt_disable(wdt);
	si5wdt_lock(wdt);

	spin_unlock(&wdt->lock);

	return 0;
}

static int si5wdt_start(struct watchdog_device *wdd)
{
	struct stf_si5_wdt *wdt = watchdog_get_drvdata(wdd);

	spin_lock(&wdt->lock);

	si5wdt_unlock(wdt);

	if (soft_noboot)
		si5wdt_disable_reset(wdt);
	else
		si5wdt_enable_reset(wdt);

	si5wdt_int_enable(wdt);

	si5wdt_set_count(wdt, wdt->count);
	si5wdt_enable(wdt);

	si5wdt_lock(wdt);

	spin_unlock(&wdt->lock);

	return 0;
}

static int si5wdt_restart(struct watchdog_device *wdd, unsigned long action,
			      void *data)
{
	struct stf_si5_wdt *wdt = watchdog_get_drvdata(wdd);

	si5wdt_unlock(wdt);
	/* disable watchdog, to be safe  */
	si5wdt_disable(wdt);

	if (soft_noboot)
		si5wdt_disable_reset(wdt);
	else
		si5wdt_enable_reset(wdt);

	/* put initial values into count and data */
	si5wdt_set_count(wdt, wdt->count);

	/* set the watchdog to go and reset... */
	si5wdt_int_clr(wdt);
	si5wdt_int_enable(wdt);
	si5wdt_enable(wdt);

	/* wait for reset to assert... */
	mdelay(500);

	si5wdt_lock(wdt);

	return 0;
}

static int si5wdt_set_timeout(struct watchdog_device *wdd,
				    unsigned int timeout)
{
	struct stf_si5_wdt *wdt = watchdog_get_drvdata(wdd);

	unsigned long freq = wdt->freq;
	unsigned int count;

	if (timeout < 1)
		return -EINVAL;

	count = timeout * freq;

	if (count > SI5_WATCHDOG_MAXCNT) {
		dev_warn(wdt->dev, "timeout %d too big,use the MAX-timeout set.\n",
				timeout);
		timeout = si5wdt_max_timeout(wdt);
		count = timeout * freq;
	}

	dev_info(wdt->dev, "Heartbeat: timeout=%d, count=%d (%08x)\n",
		timeout, count, count);

	si5wdt_unlock(wdt);
	si5wdt_disable(wdt);
	si5wdt_set_relod_count(wdt, count);
	si5wdt_enable(wdt);
	si5wdt_lock(wdt);

	wdt->count = count;
	wdd->timeout = timeout;

	return 0;
}

#define OPTIONS (WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING | WDIOF_MAGICCLOSE)

static const struct watchdog_info si5_wdt_ident = {
	.options          = OPTIONS,
	.firmware_version = 0,
	.identity         = "StarFive SI5 Watchdog",
};

static const struct watchdog_ops si5wdt_ops = {
	.owner = THIS_MODULE,
	.start = si5wdt_start,
	.stop = si5wdt_stop,
	.ping = si5wdt_keepalive,
	.set_timeout = si5wdt_set_timeout,
	.restart = si5wdt_restart,
	.get_timeleft = si5wdt_get_timeleft,
};

static const struct watchdog_device starfive_si5_wdd = {
	.info = &si5_wdt_ident,
	.ops = &si5wdt_ops,
	.timeout = SI5_WATCHDOG_DEFAULT_TIME,
};

static inline const struct si5_wdt_variant *
si5_get_wdt_drv_data(struct platform_device *pdev)
{
	const struct si5_wdt_variant *variant;

	variant = of_device_get_match_data(&pdev->dev);
	if (!variant) {
		/* Device matched by platform_device_id */
		variant = (struct si5_wdt_variant *)
			   platform_get_device_id(pdev)->driver_data;
	}

	return variant;
}

static int si5wdt_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct stf_si5_wdt *wdt;
	struct resource *wdt_irq;
	int started = 0;
	int ret;

	wdt = devm_kzalloc(dev, sizeof(*wdt), GFP_KERNEL);
	if (!wdt)
		return -ENOMEM;

	wdt->dev = dev;
	spin_lock_init(&wdt->lock);
	wdt->wdt_device = starfive_si5_wdd;

	wdt->drv_data = si5_get_wdt_drv_data(pdev);

	wdt_irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (wdt_irq == NULL) {
		dev_err(dev, "no irq resource specified\n");
		ret = -ENOENT;
		goto err;
	}

	/* get the memory region for the watchdog timer */
	wdt->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(wdt->base)) {
		ret = PTR_ERR(wdt->base);
		goto err;
	}

	wdt->freq = si5wdt_get_clock_rate(dev->of_node);

	wdt->wdt_device.min_timeout = 1;
	wdt->wdt_device.max_timeout = si5wdt_max_timeout(wdt);

	watchdog_set_drvdata(&wdt->wdt_device, wdt);

	/*
	 * see if we can actually set the requested timer margin,
	 * and if not, try the default value.
	 */
	watchdog_init_timeout(&wdt->wdt_device, tmr_margin, dev);

	ret = si5wdt_set_timeout(&wdt->wdt_device,
					wdt->wdt_device.timeout);
	if (ret) {
		dev_info(dev, "tmr_margin value out of range, default %d used\n",
				 SI5_WATCHDOG_DEFAULT_TIME);
		si5wdt_set_timeout(&wdt->wdt_device,
				SI5_WATCHDOG_DEFAULT_TIME);
	}

	ret = devm_request_irq(dev, wdt_irq->start, si5wdt_interrupt_handler, 0,
				pdev->name, pdev);
	if (ret != 0) {
		dev_err(dev, "failed to install irq (%d)\n", ret);
		goto err;
	}

	watchdog_set_nowayout(&wdt->wdt_device, nowayout);
	watchdog_set_restart_priority(&wdt->wdt_device, 128);

	wdt->wdt_device.parent = dev;

	ret = watchdog_register_device(&wdt->wdt_device);
	if (ret)
		goto err;

	ret = si5wdt_mask_and_disable_reset(wdt, false);
	if (ret < 0)
		goto err_unregister;

	if (tmr_atboot && started == 0) {
		dev_info(dev, "starting watchdog timer\n");
		si5wdt_start(&wdt->wdt_device);
	} else if (!tmr_atboot) {

		/*
		 *if we're not enabling the watchdog, then ensure it is
		 * disabled if it has been left running from the bootloader
		 * or other source.
		 */
		si5wdt_stop(&wdt->wdt_device);
	}

	platform_set_drvdata(pdev, wdt);

	return 0;

 err_unregister:
	watchdog_unregister_device(&wdt->wdt_device);

 err:
	return ret;
}

static int si5wdt_remove(struct platform_device *dev)
{
	int ret;
	struct stf_si5_wdt *wdt = platform_get_drvdata(dev);

	ret = si5wdt_mask_and_disable_reset(wdt, true);
	if (ret < 0)
		return ret;

	watchdog_unregister_device(&wdt->wdt_device);

	clk_disable_unprepare(wdt->clock);

	return 0;
}

static void si5wdt_shutdown(struct platform_device *dev)
{
	struct stf_si5_wdt *wdt = platform_get_drvdata(dev);

	si5wdt_mask_and_disable_reset(wdt, true);

	si5wdt_stop(&wdt->wdt_device);

}

#ifdef CONFIG_PM_SLEEP

static int si5wdt_suspend(struct device *dev)
{
	int ret;
	struct stf_si5_wdt *wdt = dev_get_drvdata(dev);

	si5wdt_unlock(wdt);

	/* Save watchdog state, and turn it off. */
	wdt->reload = si5wdt_get_count(wdt);

	ret = si5wdt_mask_and_disable_reset(wdt, true);
	if (ret < 0)
		return ret;

	/* Note that WTCNT doesn't need to be saved. */
	si5wdt_stop(&wdt->wdt_device);

	si5wdt_lock(wdt);

	return 0;
}

static int si5wdt_resume(struct device *dev)
{
	int ret;
	struct stf_si5_wdt *wdt = dev_get_drvdata(dev);

	si5wdt_unlock(wdt);

	/* Restore watchdog state. */
	si5wdt_set_relod_count(wdt, wdt->reload);

	ret = si5wdt_mask_and_disable_reset(wdt, false);
	if (ret < 0)
		return ret;

	si5wdt_lock(wdt);

	dev_info(dev, "watchdog resume\n")

	return 0;
}
#endif /* CONFIG_PM_SLEEP */

static SIMPLE_DEV_PM_OPS(si5wdt_pm_ops, si5wdt_suspend,
			si5wdt_resume);

static struct platform_driver starfive_si5wdt_driver = {
	.probe		= si5wdt_probe,
	.remove		= si5wdt_remove,
	.shutdown	= si5wdt_shutdown,
	.id_table	= si5wdt_ids,
	.driver		= {
		.name	= "starfive-si5-wdt",
		.pm	= &si5wdt_pm_ops,
		.of_match_table = of_match_ptr(starfive_wdt_match),
	},
};

module_platform_driver(starfive_si5wdt_driver);

MODULE_AUTHOR("samin.guo <samin.guo@starfivetech.com>");
MODULE_DESCRIPTION("StarFive SI5 Watchdog Device Driver");
MODULE_LICENSE("GPL v2");
