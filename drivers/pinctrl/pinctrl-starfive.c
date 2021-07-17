// SPDX-License-Identifier: GPL-2.0
/*
 * Pinctrl / GPIO driver for StarFive JH7100 SoC
 *
 * Copyright (C) 2021 Emil Renner Berthing <kernel@esmil.dk>
 * Copyright (C) 2021 Drew Fustini <drew@pdp7.com>
 * Copyright (C) 2020 Shanghai StarFive Technology Co., Ltd.
 */

#include <linux/gpio/driver.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <dt-bindings/pinctrl/pinctrl-starfive.h>

#include "core.h"
#include "pinctrl-utils.h"
#include "pinmux.h"
#include "pinconf.h"

#define DRIVER_NAME "pinctrl-starfive"

/*
 * refer to Section 12. GPIO Registers in JH7100 datasheet:
 * https://github.com/starfive-tech/StarLight_Docs
 */
#define MAX_GPIO	64

/*
 * Global enable for GPIO interrupts, offset: 0x0, field: GPIOEN
 * set to 1 if GPIO interrupts are enabled, set to 0 to disable
 */
#define IRQ_GLOBAL_EN		0x0

/*
 * Interrupt Type for GPIO[31:0], offset: 0x10, field: GPIOS_0
 * set to 1 if edge-triggered, set to 0 for level-triggered
 */
#define IRQ_TYPE_LOW		0x10

/*
 * Interrupt Type for GPIO[63:32], offset: 0x14, field: GPIOS_1
 */
#define IRQ_TYPE_HIGH		0x14

/*
 * Edge-Triggered Interrupt Type for GPIO[31:0], offset: 0x18, field: GPIOIBE_0
 * set to 1 if both positive and negative edge, set to 0 if single edge
 */
#define IRQ_EDGE_BOTH_LOW	0x18

/*
 * Edge-Triggered Interrupt Type for GPIO[63:32], offset: 0x1c, field: GPIOIBE_1
 */
#define IRQ_EDGE_BOTH_HIGH	0x1c

/*
 * Interrupt Trigger Polarity for GPIO[31:0], offset: 0x20, field: GPIOEV_0
 * for edge-triggered on single edge, set to 1 for rising edge, 0 for falling edge
 * for edge-triggered on both edges, this field is ignored
 * for level-triggered, set to 1 for high level, 0 for low level
 */
#define IRQ_POLARITY_LOW	0x20

/*
 * Interrupt Trigger Polarity for GPIO[63:32], offset: 0x24, field: GPIOEV_1
 */
#define IRQ_POLARITY_HIGH	0x24

/*
 * Interrupt Enable for GPIO[31:0], offset: 0x28, field: GPIOIE_0
 * set to 1 to enable (unmask) the interrupt, set to 0 to disable (mask)
 */
#define IRQ_ENABLE_LOW		0x28

/*
 * Interrupt Mask for GPIO[63:32], offset: 0x2c, field: GPIOIE_1
 */
#define IRQ_ENABLE_HIGH		0x2c

/*
 * Clear Edge-Triggered Interrupts GPIO[31:0], offset: 0x30, field: GPIOC_0
 * set to 1 to clear edge-triggered interrupt
 */
#define IRQ_CLEAR_EDGE_LOW	0x30

/*
 * Clear Edge-Triggered Interrupts GPIO[63:32], offset: 0x34, field: GPIOC_1
 */
#define IRQ_CLEAR_EDGE_HIGH	0x34

/*
 * Edge-Triggered Interrupt Status GPIO[31:0], offset: 0x38, field: GPIORIS_0
 * value of 1 means edge detected, value of 0 means no edge detected
 */
#define IRQ_EDGE_STATUS_LOW	0x38

/*
 * Edge-Triggered Interrupt Status GPIO[63:32], offset: 0x3C, field: GPIORIS_1
 */
#define IRQ_EDGE_STATUS_HIGH	0x3c

/*
 * Interrupt Status after Masking GPIO[31:0], offset: 0x40, field: GPIOMIS_0
 * status of edge-triggered or level-triggered after masking
 * value of 1 means edge or level was detected, value of 0 menas not detected
 */
#define IRQ_MASKED_STATUS_LOW	0x40

/*
 * Interrupt Status after Masking GPIO[63:32], offset: 0x44, field: GPIOMIS_1
 */
#define IRQ_MASKED_STATUS_HIGH	0x44

/*
 * Data Value of GPIO for GPIO[31:0], offest: 0x48, field: GPIODIN_0
 * dynamically reflects value on the GPIO pin
 */
#define GPIO_DIN_LOW		0x48

/*
 * Data Value of GPIO for GPIO[63:32], offest: 0x4C, field: GPIODIN_1
 */
#define GPIO_DIN_HIGH		0x4c

/*
 * From datasheet section 12.2, there are 64 output data config registers which
 * are 4 bytes wide. There are 64 output enable config registers which are 4
 * bytes wide too. Output data and output enable registers for a given GPIO pad
 * are contiguous. Thus GPIO0_DOUT_CFG is 0x50 and GPIO0_DOEN_CFG is 0x54 while
 * GPIO1_DOUT_CFG is 0x58 and GPIO1_DOEN_CFG is 0x5C. The stride between GPIO
 * GPIO pads is effectively 8, thus: GPIOn_DOUT_CFG is 0x50+8n
 */
#define GPIO_N_DOUT_CFG		0x50

/*
 * GPIO0_DOEN_CFG is 0x54, GPIOn_DOEN_CFG is 0x54+8n
 */
#define GPIO_N_DOEN_CFG		0x54

/*
 * From Section 12.3, there are 75 input signal configuration registers which
 * are 4 bytes wide starting with GPI_CPU_JTAG_TCK_CFG at 0x250 and ending with
 * GPI_USB_OVER_CURRENT_CFG 0x378
 */
#define GPIO_IN_OFFSET		0x250

/*
 * From Section 11, IO_PADSHARE_SEL register can be programmed to select one of
 * pre-defined multiplexed signal groups on PAD_FUNC_SHARE and PAD_GPIO pads.
 * This is a global setting. Per Table 11-1, setting IO_PADSHARE_SEL to 6 would
 * result in GPIO[63:0] being mapped to PAD_FUNC_SHARE[63:0]
 */
#define IO_PADSHARE_SEL		0x1a0

#define PAD_SLEW_RATE_MASK		0xe00U
#define PAD_SLEW_RATE_POS		9
#define PAD_BIAS_STRONG_PULL_UP		0x100U
#define PAD_INPUT_ENABLE		0x080U
#define PAD_INPUT_SCHMITT_ENABLE	0x040U
#define PAD_BIAS_DISABLE		0x020U
#define PAD_BIAS_PULL_DOWN		0x010U
#define PAD_BIAS_MASK			0x130U
#define PAD_DRIVE_STRENGTH_MASK		0x007U
#define PAD_DRIVE_STRENGTH_POS		0

static bool keepmux;
module_param(keepmux, bool, 0644);
MODULE_PARM_DESC(keepmux, "Keep pinmux settings from previous boot stage");

struct starfive_pinctrl {
	struct gpio_chip gc;
	struct pinctrl_gpio_range gpios;
	raw_spinlock_t lock;
	void __iomem *base;
	void __iomem *padctl;
	struct pinctrl_dev *pctl;
};

static struct device *starfive_dev(const struct starfive_pinctrl *sfp)
{
	return sfp->gc.parent;
}

static unsigned int starfive_pin_to_gpio(const struct starfive_pinctrl *sfp,
					 unsigned int pin)
{
	return pin - sfp->gpios.pin_base;
}

static unsigned int starfive_gpio_to_pin(const struct starfive_pinctrl *sfp,
					 unsigned int gpio)
{
	return gpio + sfp->gpios.pin_base;
}

static struct starfive_pinctrl *starfive_from_gc(struct gpio_chip *gc)
{
	return container_of(gc, struct starfive_pinctrl, gc);
}

static struct starfive_pinctrl *starfive_from_irq_data(struct irq_data *d)
{
	return starfive_from_gc(irq_data_get_irq_chip_data(d));
}

static struct starfive_pinctrl *starfive_from_irq_desc(struct irq_desc *desc)
{
	return starfive_from_gc(irq_desc_get_handler_data(desc));
}

static const struct pinctrl_pin_desc starfive_pins[] = {
	PINCTRL_PIN(PAD_GPIO(0), "GPIO[0]"),
	PINCTRL_PIN(PAD_GPIO(1), "GPIO[1]"),
	PINCTRL_PIN(PAD_GPIO(2), "GPIO[2]"),
	PINCTRL_PIN(PAD_GPIO(3), "GPIO[3]"),
	PINCTRL_PIN(PAD_GPIO(4), "GPIO[4]"),
	PINCTRL_PIN(PAD_GPIO(5), "GPIO[5]"),
	PINCTRL_PIN(PAD_GPIO(6), "GPIO[6]"),
	PINCTRL_PIN(PAD_GPIO(7), "GPIO[7]"),
	PINCTRL_PIN(PAD_GPIO(8), "GPIO[8]"),
	PINCTRL_PIN(PAD_GPIO(9), "GPIO[9]"),
	PINCTRL_PIN(PAD_GPIO(10), "GPIO[10]"),
	PINCTRL_PIN(PAD_GPIO(11), "GPIO[11]"),
	PINCTRL_PIN(PAD_GPIO(12), "GPIO[12]"),
	PINCTRL_PIN(PAD_GPIO(13), "GPIO[13]"),
	PINCTRL_PIN(PAD_GPIO(14), "GPIO[14]"),
	PINCTRL_PIN(PAD_GPIO(15), "GPIO[15]"),
	PINCTRL_PIN(PAD_GPIO(16), "GPIO[16]"),
	PINCTRL_PIN(PAD_GPIO(17), "GPIO[17]"),
	PINCTRL_PIN(PAD_GPIO(18), "GPIO[18]"),
	PINCTRL_PIN(PAD_GPIO(19), "GPIO[19]"),
	PINCTRL_PIN(PAD_GPIO(20), "GPIO[20]"),
	PINCTRL_PIN(PAD_GPIO(21), "GPIO[21]"),
	PINCTRL_PIN(PAD_GPIO(22), "GPIO[22]"),
	PINCTRL_PIN(PAD_GPIO(23), "GPIO[23]"),
	PINCTRL_PIN(PAD_GPIO(24), "GPIO[24]"),
	PINCTRL_PIN(PAD_GPIO(25), "GPIO[25]"),
	PINCTRL_PIN(PAD_GPIO(26), "GPIO[26]"),
	PINCTRL_PIN(PAD_GPIO(27), "GPIO[27]"),
	PINCTRL_PIN(PAD_GPIO(28), "GPIO[28]"),
	PINCTRL_PIN(PAD_GPIO(29), "GPIO[29]"),
	PINCTRL_PIN(PAD_GPIO(30), "GPIO[30]"),
	PINCTRL_PIN(PAD_GPIO(31), "GPIO[31]"),
	PINCTRL_PIN(PAD_GPIO(32), "GPIO[32]"),
	PINCTRL_PIN(PAD_GPIO(33), "GPIO[33]"),
	PINCTRL_PIN(PAD_GPIO(34), "GPIO[34]"),
	PINCTRL_PIN(PAD_GPIO(35), "GPIO[35]"),
	PINCTRL_PIN(PAD_GPIO(36), "GPIO[36]"),
	PINCTRL_PIN(PAD_GPIO(37), "GPIO[37]"),
	PINCTRL_PIN(PAD_GPIO(38), "GPIO[38]"),
	PINCTRL_PIN(PAD_GPIO(39), "GPIO[39]"),
	PINCTRL_PIN(PAD_GPIO(40), "GPIO[40]"),
	PINCTRL_PIN(PAD_GPIO(41), "GPIO[41]"),
	PINCTRL_PIN(PAD_GPIO(42), "GPIO[42]"),
	PINCTRL_PIN(PAD_GPIO(43), "GPIO[43]"),
	PINCTRL_PIN(PAD_GPIO(44), "GPIO[44]"),
	PINCTRL_PIN(PAD_GPIO(45), "GPIO[45]"),
	PINCTRL_PIN(PAD_GPIO(46), "GPIO[46]"),
	PINCTRL_PIN(PAD_GPIO(47), "GPIO[47]"),
	PINCTRL_PIN(PAD_GPIO(48), "GPIO[48]"),
	PINCTRL_PIN(PAD_GPIO(49), "GPIO[49]"),
	PINCTRL_PIN(PAD_GPIO(50), "GPIO[50]"),
	PINCTRL_PIN(PAD_GPIO(51), "GPIO[51]"),
	PINCTRL_PIN(PAD_GPIO(52), "GPIO[52]"),
	PINCTRL_PIN(PAD_GPIO(53), "GPIO[53]"),
	PINCTRL_PIN(PAD_GPIO(54), "GPIO[54]"),
	PINCTRL_PIN(PAD_GPIO(55), "GPIO[55]"),
	PINCTRL_PIN(PAD_GPIO(56), "GPIO[56]"),
	PINCTRL_PIN(PAD_GPIO(57), "GPIO[57]"),
	PINCTRL_PIN(PAD_GPIO(58), "GPIO[58]"),
	PINCTRL_PIN(PAD_GPIO(59), "GPIO[59]"),
	PINCTRL_PIN(PAD_GPIO(60), "GPIO[60]"),
	PINCTRL_PIN(PAD_GPIO(61), "GPIO[61]"),
	PINCTRL_PIN(PAD_GPIO(62), "GPIO[62]"),
	PINCTRL_PIN(PAD_GPIO(63), "GPIO[63]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(0), "FUNC_SHARE[0]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(1), "FUNC_SHARE[1]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(2), "FUNC_SHARE[2]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(3), "FUNC_SHARE[3]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(4), "FUNC_SHARE[4]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(5), "FUNC_SHARE[5]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(6), "FUNC_SHARE[6]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(7), "FUNC_SHARE[7]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(8), "FUNC_SHARE[8]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(9), "FUNC_SHARE[9]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(10), "FUNC_SHARE[10]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(11), "FUNC_SHARE[11]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(12), "FUNC_SHARE[12]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(13), "FUNC_SHARE[13]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(14), "FUNC_SHARE[14]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(15), "FUNC_SHARE[15]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(16), "FUNC_SHARE[16]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(17), "FUNC_SHARE[17]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(18), "FUNC_SHARE[18]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(19), "FUNC_SHARE[19]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(20), "FUNC_SHARE[20]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(21), "FUNC_SHARE[21]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(22), "FUNC_SHARE[22]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(23), "FUNC_SHARE[23]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(24), "FUNC_SHARE[24]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(25), "FUNC_SHARE[25]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(26), "FUNC_SHARE[26]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(27), "FUNC_SHARE[27]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(28), "FUNC_SHARE[28]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(29), "FUNC_SHARE[29]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(30), "FUNC_SHARE[30]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(31), "FUNC_SHARE[31]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(32), "FUNC_SHARE[32]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(33), "FUNC_SHARE[33]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(34), "FUNC_SHARE[34]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(35), "FUNC_SHARE[35]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(36), "FUNC_SHARE[36]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(37), "FUNC_SHARE[37]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(38), "FUNC_SHARE[38]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(39), "FUNC_SHARE[39]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(40), "FUNC_SHARE[40]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(41), "FUNC_SHARE[41]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(42), "FUNC_SHARE[42]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(43), "FUNC_SHARE[43]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(44), "FUNC_SHARE[44]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(45), "FUNC_SHARE[45]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(46), "FUNC_SHARE[46]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(47), "FUNC_SHARE[47]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(48), "FUNC_SHARE[48]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(49), "FUNC_SHARE[49]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(50), "FUNC_SHARE[50]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(51), "FUNC_SHARE[51]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(52), "FUNC_SHARE[52]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(53), "FUNC_SHARE[53]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(54), "FUNC_SHARE[54]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(55), "FUNC_SHARE[55]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(56), "FUNC_SHARE[56]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(57), "FUNC_SHARE[57]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(58), "FUNC_SHARE[58]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(59), "FUNC_SHARE[59]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(60), "FUNC_SHARE[60]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(61), "FUNC_SHARE[61]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(62), "FUNC_SHARE[62]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(63), "FUNC_SHARE[63]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(64), "FUNC_SHARE[64]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(65), "FUNC_SHARE[65]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(66), "FUNC_SHARE[66]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(67), "FUNC_SHARE[67]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(68), "FUNC_SHARE[68]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(69), "FUNC_SHARE[69]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(70), "FUNC_SHARE[70]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(71), "FUNC_SHARE[71]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(72), "FUNC_SHARE[72]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(73), "FUNC_SHARE[73]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(74), "FUNC_SHARE[74]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(75), "FUNC_SHARE[75]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(76), "FUNC_SHARE[76]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(77), "FUNC_SHARE[77]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(78), "FUNC_SHARE[78]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(79), "FUNC_SHARE[79]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(80), "FUNC_SHARE[80]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(81), "FUNC_SHARE[81]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(82), "FUNC_SHARE[82]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(83), "FUNC_SHARE[83]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(84), "FUNC_SHARE[84]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(85), "FUNC_SHARE[85]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(86), "FUNC_SHARE[86]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(87), "FUNC_SHARE[87]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(88), "FUNC_SHARE[88]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(89), "FUNC_SHARE[89]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(90), "FUNC_SHARE[90]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(91), "FUNC_SHARE[91]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(92), "FUNC_SHARE[92]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(93), "FUNC_SHARE[93]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(94), "FUNC_SHARE[94]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(95), "FUNC_SHARE[95]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(96), "FUNC_SHARE[96]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(97), "FUNC_SHARE[97]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(98), "FUNC_SHARE[98]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(99), "FUNC_SHARE[99]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(100), "FUNC_SHARE[100]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(101), "FUNC_SHARE[101]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(102), "FUNC_SHARE[102]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(103), "FUNC_SHARE[103]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(104), "FUNC_SHARE[104]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(105), "FUNC_SHARE[105]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(106), "FUNC_SHARE[106]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(107), "FUNC_SHARE[107]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(108), "FUNC_SHARE[108]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(109), "FUNC_SHARE[109]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(110), "FUNC_SHARE[110]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(111), "FUNC_SHARE[111]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(112), "FUNC_SHARE[112]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(113), "FUNC_SHARE[113]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(114), "FUNC_SHARE[114]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(115), "FUNC_SHARE[115]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(116), "FUNC_SHARE[116]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(117), "FUNC_SHARE[117]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(118), "FUNC_SHARE[118]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(119), "FUNC_SHARE[119]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(120), "FUNC_SHARE[120]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(121), "FUNC_SHARE[121]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(122), "FUNC_SHARE[122]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(123), "FUNC_SHARE[123]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(124), "FUNC_SHARE[124]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(125), "FUNC_SHARE[125]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(126), "FUNC_SHARE[126]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(127), "FUNC_SHARE[127]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(128), "FUNC_SHARE[128]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(129), "FUNC_SHARE[129]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(130), "FUNC_SHARE[130]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(131), "FUNC_SHARE[131]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(132), "FUNC_SHARE[132]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(133), "FUNC_SHARE[133]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(134), "FUNC_SHARE[134]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(135), "FUNC_SHARE[135]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(136), "FUNC_SHARE[136]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(137), "FUNC_SHARE[137]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(138), "FUNC_SHARE[138]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(139), "FUNC_SHARE[139]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(140), "FUNC_SHARE[140]"),
	PINCTRL_PIN(PAD_FUNC_SHARE(141), "FUNC_SHARE[141]"),
};

#ifdef CONFIG_DEBUG_FS
static void starfive_pin_dbg_show(struct pinctrl_dev *pctldev,
				  struct seq_file *s,
				  unsigned int pin)
{
	struct starfive_pinctrl *sfp = pinctrl_dev_get_drvdata(pctldev);
	unsigned int gpio = starfive_pin_to_gpio(sfp, pin);
	void __iomem *reg;
	u32 dout, doen;

	if (gpio >= MAX_GPIO)
		return;

	reg = sfp->base + GPIO_N_DOUT_CFG + 8 * gpio;
	dout = readl_relaxed(reg);
	reg += 4;
	doen = readl_relaxed(reg);

	seq_printf(s, "dout=%u%s doen=%u%s",
		   dout & 0xffU, (dout & 0x80000000U) ? "r" : "",
		   doen & 0xffU, (doen & 0x80000000U) ? "r" : "");
}
#else
#define starfive_pin_dbg_show NULL
#endif

static int starfive_dt_node_to_map(struct pinctrl_dev *pctldev,
				   struct device_node *np,
				   struct pinctrl_map **maps,
				   unsigned int *num_maps)
{
	struct starfive_pinctrl *sfp = pinctrl_dev_get_drvdata(pctldev);
	struct device *dev = starfive_dev(sfp);
	const char **pgnames;
	struct pinctrl_map *map;
	struct device_node *child;
	const char *grpname;
	unsigned int *pins;
	u32 *pinmux;
	int nmaps;
	int ngroups;
	int ret;

	nmaps = 0;
	ngroups = 0;
	for_each_child_of_node(np, child) {
		const __be32 *pinmux;
		const __be32 *pins;
		int pinmux_size;
		int pins_size;

		pinmux = of_get_property(child, "pinmux", &pinmux_size);
		pins   = of_get_property(child, "pins",   &pins_size);
		if (pinmux && pins) {
			dev_err(dev, "invalid pinctrl group %pOFn.%pOFn: %s\n",
				np, child, "both pinmux and pins set");
			of_node_put(child);
			return -EINVAL;
		}

		if (pinmux && pinmux_size > 0) {
			nmaps += 2;
		} else if (pins && pins_size > 0) {
			nmaps += 1;
		} else {
			dev_err(dev, "invalid pinctrl group %pOFn.%pOFn: %s\n",
				np, child, "neither pinmux nor pins set");
			of_node_put(child);
			return -EINVAL;
		}
		ngroups += 1;
	}

	ret = -ENOMEM;
	pgnames = devm_kcalloc(dev, ngroups, sizeof(*pgnames), GFP_KERNEL);
	if (!pgnames)
		goto out;

	map = kcalloc(nmaps, sizeof(*map), GFP_KERNEL);
	if (!map)
		goto free_pgnames;

	nmaps = 0;
	ngroups = 0;
	for_each_child_of_node(np, child) {
		const __be32 *list;
		int npins;
		int i;

		ret = -ENOMEM;
		grpname = devm_kasprintf(dev, GFP_KERNEL, "%s.%s", np->name, child->name);
		if (!grpname)
			goto put_child;

		pgnames[ngroups++] = grpname;

		if ((list = of_get_property(child, "pinmux", &npins))) {
			npins /= sizeof(*list);

			pins = devm_kcalloc(dev, npins, sizeof(*pins), GFP_KERNEL);
			if (!pins)
				goto free_grpname;

			pinmux = devm_kcalloc(dev, npins, sizeof(*pinmux), GFP_KERNEL);
			if (!pinmux)
				goto free_pins;

			for (i = 0; i < npins; i++) {
				u32 v = be32_to_cpu(*list++);

				pins[i] = starfive_gpio_to_pin(sfp, v & (MAX_GPIO - 1));
				pinmux[i] = v;
			}

			map[nmaps].type = PIN_MAP_TYPE_MUX_GROUP;
			map[nmaps].data.mux.function = np->name;
			map[nmaps].data.mux.group = grpname;
			nmaps += 1;
		} else if ((list = of_get_property(child, "pins", &npins))) {
			npins /= sizeof(*list);

			pins = devm_kcalloc(dev, npins, sizeof(*pins), GFP_KERNEL);
			if (!pins)
				goto free_grpname;

			pinmux = NULL;

			for (i = 0; i < npins; i++)
				pins[i] = be32_to_cpu(*list++);
		} else {
			ret = -EINVAL;
			goto free_grpname;
		}

		ret = pinctrl_generic_add_group(pctldev, grpname, pins, npins, pinmux);
		if (ret < 0) {
			dev_err(dev, "error adding group %pOFn.%pOFn: %d\n",
				np, child, ret);
			goto free_pinmux;
		}

		ret = pinconf_generic_parse_dt_config(child, pctldev,
				&map[nmaps].data.configs.configs,
				&map[nmaps].data.configs.num_configs);
		if (ret) {
			dev_err(dev, "invalid pinctrl group %pOFn.%pOFn: %s\n",
				np, child, "error parsing pin config");
			goto put_child;
		}

		/* don't create a map if there are no pinconf settings */
		if (map[nmaps].data.configs.num_configs == 0)
			continue;

		map[nmaps].type = PIN_MAP_TYPE_CONFIGS_GROUP;
		map[nmaps].data.configs.group_or_pin = grpname;
		nmaps += 1;
	}

	ret = pinmux_generic_add_function(pctldev, np->name, pgnames, ngroups, NULL);
	if (ret < 0) {
		dev_err(dev, "error adding function %pOFn: %d\n", np, ret);
		goto free_map;
	}

	*maps = map;
	*num_maps = nmaps;
	return 0;

free_pinmux:
	devm_kfree(dev, pinmux);
free_pins:
	devm_kfree(dev, pins);
free_grpname:
	devm_kfree(dev, grpname);
put_child:
	of_node_put(child);
free_map:
	pinctrl_utils_free_map(pctldev, map, nmaps);
free_pgnames:
	devm_kfree(dev, pgnames);
out:
	return ret;
}

static const struct pinctrl_ops starfive_pinctrl_ops = {
	.get_groups_count = pinctrl_generic_get_group_count,
	.get_group_name = pinctrl_generic_get_group_name,
	.get_group_pins = pinctrl_generic_get_group_pins,
	.pin_dbg_show = starfive_pin_dbg_show,
	.dt_node_to_map = starfive_dt_node_to_map,
	.dt_free_map = pinctrl_utils_free_map,
};

static int starfive_set_mux(struct pinctrl_dev *pctldev,
			    unsigned int fsel, unsigned int gsel)
{
	struct starfive_pinctrl *sfp = pinctrl_dev_get_drvdata(pctldev);
	struct device *dev = starfive_dev(sfp);
	const struct group_desc *group;
	const u32 *pinmux;
	unsigned int i;

	group = pinctrl_generic_get_group(pctldev, gsel);
	if (unlikely(!group))
		return -EINVAL;

	pinmux = group->data;
	for (i = 0; i < group->num_pins; i++) {
		unsigned int gpio = starfive_pin_to_gpio(sfp, group->pins[i]);
		void __iomem *reg_dout;
		void __iomem *reg_doen;
		void __iomem *reg_din;
		u32 v, dout, doen, din;
		unsigned long flags;

		if (dev_WARN_ONCE(dev, gpio >= MAX_GPIO,
				  "%s: invalid gpiomux pin", group->name))
			continue;

		v = pinmux[i];
		dout = ((v & BIT(7)) << (31 - 7)) | ((v >> 24) & 0xffU);
		doen = ((v & BIT(6)) << (31 - 6)) | ((v >> 16) & 0xffU);
		din  = (v >> 8) & 0xffU;

		dev_dbg(dev, "GPIO%u: dout=0x%x doen=0x%x din=0x%x\n",
			gpio, dout, doen, din);

		reg_dout = sfp->base + GPIO_N_DOUT_CFG + 8 * gpio;
		reg_doen = sfp->base + GPIO_N_DOEN_CFG + 8 * gpio;
		if (din != 0xff)
			reg_din = sfp->base + GPIO_IN_OFFSET + 4 * din;
		else
			reg_din = NULL;

		raw_spin_lock_irqsave(&sfp->lock, flags);
		writel_relaxed(dout, reg_dout);
		writel_relaxed(doen, reg_doen);
		if (reg_din)
			writel_relaxed(gpio + 2, reg_din);
		raw_spin_unlock_irqrestore(&sfp->lock, flags);
	}

	return 0;
}

static const struct pinmux_ops starfive_pinmux_ops = {
	.get_functions_count = pinmux_generic_get_function_count,
	.get_function_name = pinmux_generic_get_function_name,
	.get_function_groups = pinmux_generic_get_function_groups,
	.set_mux = starfive_set_mux,
	.strict = true,
};

static u16 starfive_padctl_get(struct starfive_pinctrl *sfp,
			       unsigned int pin)
{
	void __iomem *reg = sfp->padctl + 4 * (pin / 2);
	u32 value = readl_relaxed(reg);

	if (pin & 1U)
		value >>= 16;
	return value;
}

static void starfive_padctl_rmw(struct starfive_pinctrl *sfp,
				unsigned int pin,
				u16 _mask, u16 _value)
{
	void __iomem *reg = sfp->padctl + 4 * (pin / 2);
	u32 mask = _mask;
	u32 value = _value;
	unsigned long flags;

	dev_dbg(starfive_dev(sfp),
		"padctl_rmw(%u, 0x%03x, 0x%03x)\n", pin, mask, value);

	if (pin & 1U) {
		value <<= 16;
		mask <<= 16;
	}

	raw_spin_lock_irqsave(&sfp->lock, flags);
	value |= readl_relaxed(reg) & ~mask;
	writel_relaxed(value, reg);
	raw_spin_unlock_irqrestore(&sfp->lock, flags);
}

#define PIN_CONFIG_STARFIVE_STRONG_PULL_UP	(PIN_CONFIG_END + 1)

static const struct pinconf_generic_params starfive_pinconf_custom_params[] = {
	{ "starfive,strong-pull-up", PIN_CONFIG_STARFIVE_STRONG_PULL_UP, 1 },
};

#ifdef CONFIG_DEBUG_FS
static const struct pin_config_item
starfive_pinconf_custom_conf_items[ARRAY_SIZE(starfive_pinconf_custom_params)] = {
	PCONFDUMP(PIN_CONFIG_STARFIVE_STRONG_PULL_UP, "input bias strong pull-up", NULL, false),
};
#else
#define starfive_pinconf_custom_conf_items NULL
#endif

static const unsigned char starfive_drive_strength[] = {
	14, 21, 28, 35, 42, 49, 56, 63,
};

static int starfive_pinconf_get(struct pinctrl_dev *pctldev,
				unsigned int pin, unsigned long *config)
{
	struct starfive_pinctrl *sfp = pinctrl_dev_get_drvdata(pctldev);
	u16 value = starfive_padctl_get(sfp, pin);
	int param = pinconf_to_config_param(*config);
	u32 arg;
	bool enabled;

	switch (param) {
	case PIN_CONFIG_BIAS_DISABLE:
		enabled = value & PAD_BIAS_DISABLE;
		arg = 0;
		break;
	case PIN_CONFIG_BIAS_PULL_DOWN:
		enabled = value & PAD_BIAS_PULL_DOWN;
		arg = 1;
		break;
	case PIN_CONFIG_BIAS_PULL_UP:
		enabled = !(value & PAD_BIAS_MASK);
		arg = 1;
		break;
	case PIN_CONFIG_DRIVE_STRENGTH:
		enabled = value & PAD_DRIVE_STRENGTH_MASK;
		arg = starfive_drive_strength[value & PAD_DRIVE_STRENGTH_MASK];
		break;
	case PIN_CONFIG_INPUT_ENABLE:
		enabled = value & PAD_INPUT_ENABLE;
		arg = enabled;
		break;
	case PIN_CONFIG_INPUT_SCHMITT_ENABLE:
		enabled = value & PAD_INPUT_SCHMITT_ENABLE;
		arg = enabled;
		break;
	case PIN_CONFIG_SLEW_RATE:
		enabled = value & PAD_SLEW_RATE_MASK;
		arg = (value & PAD_SLEW_RATE_MASK) >> PAD_SLEW_RATE_POS;
		break;
	case PIN_CONFIG_STARFIVE_STRONG_PULL_UP:
		enabled = value & PAD_BIAS_STRONG_PULL_UP;
		arg = enabled;
		break;
	default:
		return -ENOTSUPP;
	}

	*config = pinconf_to_config_packed(param, arg);
	return enabled ? 0 : -EINVAL;
}

static int starfive_pinconf_group_get(struct pinctrl_dev *pctldev,
				      unsigned int gsel, unsigned long *config)
{
	const struct group_desc *group;

	group = pinctrl_generic_get_group(pctldev, gsel);
	if (unlikely(!group))
		return -EINVAL;

	return starfive_pinconf_get(pctldev, group->pins[0], config);
}

static int starfive_pinconf_group_set(struct pinctrl_dev *pctldev,
				      unsigned int gsel,
				      unsigned long *configs,
				      unsigned int num_configs)
{
	struct starfive_pinctrl *sfp = pinctrl_dev_get_drvdata(pctldev);
	const struct group_desc *group;
	u16 mask, value;
	int i;

	group = pinctrl_generic_get_group(pctldev, gsel);
	if (unlikely(!group))
		return -EINVAL;

	mask = 0;
	value = 0;
	for (i = 0; i < num_configs; i++) {
		int param = pinconf_to_config_param(configs[i]);
		u32 arg = pinconf_to_config_argument(configs[i]);
		u16 ds;

		switch (param) {
		case PIN_CONFIG_BIAS_DISABLE:
			mask |= PAD_BIAS_MASK;
			value = (value & ~PAD_BIAS_MASK) | PAD_BIAS_DISABLE;
			break;
		case PIN_CONFIG_BIAS_PULL_DOWN:
			if (arg == 0)
				return -ENOTSUPP;
			mask |= PAD_BIAS_MASK;
			value = (value & ~PAD_BIAS_MASK) | PAD_BIAS_PULL_DOWN;
			break;
		case PIN_CONFIG_BIAS_PULL_UP:
			if (arg == 0)
				return -ENOTSUPP;
			mask |= PAD_BIAS_MASK;
			value = value & ~PAD_BIAS_MASK;
			break;
		case PIN_CONFIG_DRIVE_STRENGTH:
			for (ds = 0; ds < PAD_DRIVE_STRENGTH_MASK; ds++) {
				if (arg < starfive_drive_strength[ds + 1])
					break;
			}
			mask |= PAD_DRIVE_STRENGTH_MASK;
			value = (value & ~PAD_DRIVE_STRENGTH_MASK) | ds;
			break;
		case PIN_CONFIG_INPUT_ENABLE:
			mask |= PAD_INPUT_ENABLE;
			if (arg)
				value |= PAD_INPUT_ENABLE;
			else
				value &= ~PAD_INPUT_ENABLE;
			break;
		case PIN_CONFIG_INPUT_SCHMITT_ENABLE:
			mask |= PAD_INPUT_SCHMITT_ENABLE;
			if (arg)
				value |= PAD_INPUT_SCHMITT_ENABLE;
			else
				value &= ~PAD_INPUT_SCHMITT_ENABLE;
			break;
		case PIN_CONFIG_SLEW_RATE:
			mask |= PAD_SLEW_RATE_MASK;
			value = (value & ~PAD_SLEW_RATE_MASK) |
				((arg << PAD_SLEW_RATE_POS) & PAD_SLEW_RATE_MASK);
			break;
		case PIN_CONFIG_STARFIVE_STRONG_PULL_UP:
			if (arg) {
				mask |= PAD_BIAS_MASK;
				value = (value & ~PAD_BIAS_MASK) |
					PAD_BIAS_STRONG_PULL_UP;
			} else {
				mask |= PAD_BIAS_STRONG_PULL_UP;
				value = value & ~PAD_BIAS_STRONG_PULL_UP;
			}
			break;
		default:
			return -ENOTSUPP;
		}
	}

	for (i = 0; i < group->num_pins; i++)
		starfive_padctl_rmw(sfp, group->pins[i], mask, value);

	return 0;
}

#ifdef CONFIG_DEBUG_FS
static void starfive_pinconf_dbg_show(struct pinctrl_dev *pctldev,
				      struct seq_file *s, unsigned int pin)
{
	struct starfive_pinctrl *sfp = pinctrl_dev_get_drvdata(pctldev);
	u16 value = starfive_padctl_get(sfp, pin);

	seq_printf(s, " (0x%03x)", value);
}
#else
#define starfive_pinconf_dbg_show NULL
#endif

static const struct pinconf_ops starfive_pinconf_ops = {
	.pin_config_get = starfive_pinconf_get,
	.pin_config_group_get = starfive_pinconf_group_get,
	.pin_config_group_set = starfive_pinconf_group_set,
	.pin_config_dbg_show = starfive_pinconf_dbg_show,
	.is_generic = true,
};

static struct pinctrl_desc starfive_desc = {
	.name = DRIVER_NAME,
	.pins = starfive_pins,
	.npins = ARRAY_SIZE(starfive_pins),
	.pctlops = &starfive_pinctrl_ops,
	.pmxops = &starfive_pinmux_ops,
	.confops = &starfive_pinconf_ops,
	.owner = THIS_MODULE,
	.num_custom_params = ARRAY_SIZE(starfive_pinconf_custom_params),
	.custom_params = starfive_pinconf_custom_params,
	.custom_conf_items = starfive_pinconf_custom_conf_items,
};

static int starfive_gpio_request(struct gpio_chip *gc, unsigned int gpio)
{
	return pinctrl_gpio_request(gc->base + gpio);
}

static void starfive_gpio_free(struct gpio_chip *gc, unsigned int gpio)
{
	pinctrl_gpio_free(gc->base + gpio);
}

static int starfive_gpio_get_direction(struct gpio_chip *gc, unsigned int gpio)
{
	struct starfive_pinctrl *sfp = starfive_from_gc(gc);

	if (gpio >= MAX_GPIO)
		return -EINVAL;

	/* return GPIO_LINE_DIRECTION_OUT (0) only if doen == GPO_ENABLE (0) */
	return readl_relaxed(sfp->base + GPIO_N_DOEN_CFG + 8 * gpio) != GPO_ENABLE;
}

static int starfive_gpio_direction_input(struct gpio_chip *gc,
					 unsigned int gpio)
{
	struct starfive_pinctrl *sfp = starfive_from_gc(gc);
	unsigned long flags;

	if (gpio >= MAX_GPIO)
		return -EINVAL;

	/* enable input and schmitt trigger */
	starfive_padctl_rmw(sfp, starfive_gpio_to_pin(sfp, gpio),
			PAD_INPUT_ENABLE | PAD_INPUT_SCHMITT_ENABLE,
			PAD_INPUT_ENABLE | PAD_INPUT_SCHMITT_ENABLE);

	raw_spin_lock_irqsave(&sfp->lock, flags);
	writel_relaxed(GPO_DISABLE, sfp->base + GPIO_N_DOEN_CFG + 8 * gpio);
	raw_spin_unlock_irqrestore(&sfp->lock, flags);

	return 0;
}

static int starfive_gpio_direction_output(struct gpio_chip *gc,
					  unsigned int gpio, int value)
{
	struct starfive_pinctrl *sfp = starfive_from_gc(gc);
	unsigned long flags;

	if (gpio >= MAX_GPIO)
		return -EINVAL;

	raw_spin_lock_irqsave(&sfp->lock, flags);
	writel_relaxed(value, sfp->base + GPIO_N_DOUT_CFG + 8 * gpio);
	writel_relaxed(GPO_ENABLE, sfp->base + GPIO_N_DOEN_CFG + 8 * gpio);
	raw_spin_unlock_irqrestore(&sfp->lock, flags);

	/* disable input, schmitt trigger and bias */
	starfive_padctl_rmw(sfp, starfive_gpio_to_pin(sfp, gpio),
			PAD_BIAS_MASK | PAD_INPUT_ENABLE | PAD_INPUT_SCHMITT_ENABLE,
			PAD_BIAS_DISABLE);

	return 0;
}

static int starfive_gpio_get(struct gpio_chip *gc, unsigned int gpio)
{
	struct starfive_pinctrl *sfp = starfive_from_gc(gc);
	u32 value;

	if (gpio >= MAX_GPIO)
		return -EINVAL;

	if (gpio < 32) {
		value = readl_relaxed(sfp->base + GPIO_DIN_LOW);
		value = (value >> gpio) & 1U;
	} else {
		value = readl_relaxed(sfp->base + GPIO_DIN_HIGH);
		value = (value >> (gpio - 32)) & 1U;
	}

	return value;
}

static void starfive_gpio_set(struct gpio_chip *gc, unsigned int gpio,
			      int value)
{
	struct starfive_pinctrl *sfp = starfive_from_gc(gc);
	unsigned long flags;

	if (gpio >= MAX_GPIO)
		return;

	raw_spin_lock_irqsave(&sfp->lock, flags);
	writel_relaxed(value, sfp->base + GPIO_N_DOUT_CFG + 8 * gpio);
	raw_spin_unlock_irqrestore(&sfp->lock, flags);
}

static int starfive_gpio_set_config(struct gpio_chip *gc, unsigned int gpio,
				    unsigned long config)
{
	struct starfive_pinctrl *sfp = starfive_from_gc(gc);
	u32 arg = pinconf_to_config_argument(config);
	u16 mask;
	u16 value;

	switch (pinconf_to_config_param(config)) {
	case PIN_CONFIG_BIAS_DISABLE:
		mask  = PAD_BIAS_MASK;
		value = PAD_BIAS_DISABLE;
		break;
	case PIN_CONFIG_BIAS_PULL_DOWN:
		if (arg == 0)
			return -ENOTSUPP;
		mask  = PAD_BIAS_MASK;
		value = PAD_BIAS_PULL_DOWN;
		break;
	case PIN_CONFIG_BIAS_PULL_UP:
		if (arg == 0)
			return -ENOTSUPP;
		mask  = PAD_BIAS_MASK;
		value = 0;
		break;
	case PIN_CONFIG_DRIVE_PUSH_PULL:
		return 0;
	case PIN_CONFIG_INPUT_ENABLE:
		mask  = PAD_INPUT_ENABLE;
		value = arg ? PAD_INPUT_ENABLE : 0;
		break;
	case PIN_CONFIG_INPUT_SCHMITT_ENABLE:
		mask  = PAD_INPUT_SCHMITT_ENABLE;
		value = arg ? PAD_INPUT_SCHMITT_ENABLE : 0;
		break;
	default:
		return -ENOTSUPP;
	};

	starfive_padctl_rmw(sfp, starfive_gpio_to_pin(sfp, gpio), mask, value);
	return 0;
}

static int starfive_gpio_add_pin_ranges(struct gpio_chip *gc)
{
	struct starfive_pinctrl *sfp = starfive_from_gc(gc);

	sfp->gpios.name = sfp->gc.label;
	sfp->gpios.base = sfp->gc.base;
	/*
	 * sfp->gpios.pin_base depends on the chosen signal group
	 * and is set in starfive_probe()
	 */
	sfp->gpios.npins = MAX_GPIO;
	sfp->gpios.gc = &sfp->gc;
	pinctrl_add_gpio_range(sfp->pctl, &sfp->gpios);
	return 0;
}

static void starfive_irq_ack(struct irq_data *d)
{
	struct starfive_pinctrl *sfp = starfive_from_irq_data(d);
	irq_hw_number_t gpio = irqd_to_hwirq(d);
	unsigned long flags;
	void __iomem *ic;
	u32 mask;

	if (gpio < 0 || gpio >= MAX_GPIO)
		return;

	if (gpio < 32) {
		ic = sfp->base + IRQ_CLEAR_EDGE_LOW;
		mask = BIT(gpio);
	} else {
		ic = sfp->base + IRQ_CLEAR_EDGE_HIGH;
		mask = BIT(gpio - 32);
	}

	raw_spin_lock_irqsave(&sfp->lock, flags);
	writel_relaxed(mask, ic);
	raw_spin_unlock_irqrestore(&sfp->lock, flags);
}

static void starfive_irq_mask(struct irq_data *d)
{
	struct starfive_pinctrl *sfp = starfive_from_irq_data(d);
	irq_hw_number_t gpio = irqd_to_hwirq(d);
	unsigned long flags;
	void __iomem *ie;
	u32 mask, value;

	if (gpio < 0 || gpio >= MAX_GPIO)
		return;

	if (gpio < 32) {
		ie = sfp->base + IRQ_ENABLE_LOW;
		mask = BIT(gpio);
	} else {
		ie = sfp->base + IRQ_ENABLE_HIGH;
		mask = BIT(gpio - 32);
	}

	raw_spin_lock_irqsave(&sfp->lock, flags);
	value = readl_relaxed(ie);
	value &= ~mask;
	writel_relaxed(value, ie);
	raw_spin_unlock_irqrestore(&sfp->lock, flags);
}

static void starfive_irq_mask_ack(struct irq_data *d)
{
	struct starfive_pinctrl *sfp = starfive_from_irq_data(d);
	irq_hw_number_t gpio = irqd_to_hwirq(d);
	unsigned long flags;
	void __iomem *ie;
	void __iomem *ic;
	u32 mask, value;

	if (gpio < 0 || gpio >= MAX_GPIO)
		return;

	if (gpio < 32) {
		ie = sfp->base + IRQ_ENABLE_LOW;
		ic = sfp->base + IRQ_CLEAR_EDGE_LOW;
		mask = BIT(gpio);
	} else {
		ie = sfp->base + IRQ_ENABLE_HIGH;
		ic = sfp->base + IRQ_CLEAR_EDGE_HIGH;
		mask = BIT(gpio - 32);
	}

	raw_spin_lock_irqsave(&sfp->lock, flags);
	value = readl_relaxed(ie);
	value &= ~mask;
	writel_relaxed(value, ie);
	writel_relaxed(mask, ic);
	raw_spin_unlock_irqrestore(&sfp->lock, flags);
}

static void starfive_irq_unmask(struct irq_data *d)
{
	struct starfive_pinctrl *sfp = starfive_from_irq_data(d);
	irq_hw_number_t gpio = irqd_to_hwirq(d);
	unsigned long flags;
	void __iomem *ie;
	u32 mask, value;

	if (gpio < 0 || gpio >= MAX_GPIO)
		return;

	if (gpio < 32) {
		ie = sfp->base + IRQ_ENABLE_LOW;
		mask = BIT(gpio);
	} else {
		ie = sfp->base + IRQ_ENABLE_HIGH;
		mask = BIT(gpio - 32);
	}

	raw_spin_lock_irqsave(&sfp->lock, flags);
	value = readl_relaxed(ie);
	value |= mask;
	writel_relaxed(value, ie);
	raw_spin_unlock_irqrestore(&sfp->lock, flags);
}

static int starfive_irq_set_type(struct irq_data *d, unsigned int trigger)
{
	struct starfive_pinctrl *sfp = starfive_from_irq_data(d);
	irq_hw_number_t gpio = irqd_to_hwirq(d);
	unsigned long flags;
	void __iomem *base;
	u32 mask, irq_type, edge_both, polarity;

	if (gpio < 0 || gpio >= MAX_GPIO)
		return -EINVAL;

	if (gpio < 32) {
		base = sfp->base;
		mask = BIT(gpio);
	} else {
		base = sfp->base + 4;
		mask = BIT(gpio - 32);
	}

	switch (trigger) {
	case IRQ_TYPE_EDGE_RISING:
		irq_set_handler_locked(d, handle_edge_irq);
		irq_type  = mask; /* 1: edge triggered */
		edge_both = 0;    /* 0: single edge */
		polarity  = mask; /* 1: rising edge */
		break;
	case IRQ_TYPE_EDGE_FALLING:
		irq_set_handler_locked(d, handle_edge_irq);
		irq_type  = mask; /* 1: edge triggered */
		edge_both = 0;    /* 0: single edge */
		polarity  = 0;    /* 0: falling edge */
		break;
	case IRQ_TYPE_EDGE_BOTH:
		irq_set_handler_locked(d, handle_edge_irq);
		irq_type  = mask; /* 1: edge triggered */
		edge_both = mask; /* 1: both edges */
		polarity  = 0;    /* 0: ignored */
		break;
	case IRQ_TYPE_LEVEL_HIGH:
		irq_set_handler_locked(d, handle_level_irq);
		irq_type  = 0;    /* 0: level trigged */
		edge_both = 0;    /* 0: ignored */
		polarity  = mask; /* 1: high level */
		break;
	case IRQ_TYPE_LEVEL_LOW:
		irq_set_handler_locked(d, handle_level_irq);
		irq_type  = 0;    /* 0: level triggered */
		edge_both = 0;    /* 0: ignored */
		polarity  = 0;    /* 0: low level */
		break;
	default:
		irq_set_handler_locked(d, handle_bad_irq);
		return -ENOTSUPP;
	}

	raw_spin_lock_irqsave(&sfp->lock, flags);
	irq_type |= readl_relaxed(base + IRQ_TYPE_LOW) & ~mask;
	writel_relaxed(irq_type, base + IRQ_TYPE_LOW);
	edge_both |= readl_relaxed(base + IRQ_EDGE_BOTH_LOW) & ~mask;
	writel_relaxed(edge_both, base + IRQ_EDGE_BOTH_LOW);
	polarity |= readl_relaxed(base + IRQ_POLARITY_LOW) & ~mask;
	writel_relaxed(polarity, base + IRQ_POLARITY_LOW);
	raw_spin_unlock_irqrestore(&sfp->lock, flags);
	return 0;
}

static struct irq_chip starfive_irq_chip = {
	.irq_ack = starfive_irq_ack,
	.irq_mask = starfive_irq_mask,
	.irq_mask_ack = starfive_irq_mask_ack,
	.irq_unmask = starfive_irq_unmask,
	.irq_set_type = starfive_irq_set_type,
	.flags = IRQCHIP_SET_TYPE_MASKED,
};

static void starfive_gpio_irq_handler(struct irq_desc *desc)
{
	struct starfive_pinctrl *sfp = starfive_from_irq_desc(desc);
	struct irq_chip *chip = irq_desc_get_chip(desc);
	unsigned long mis;
	unsigned int pin;

	chained_irq_enter(chip, desc);

	mis = readl_relaxed(sfp->base + IRQ_MASKED_STATUS_LOW);
	for_each_set_bit(pin, &mis, 32)
		generic_handle_domain_irq(sfp->gc.irq.domain, pin);

	mis = readl_relaxed(sfp->base + IRQ_MASKED_STATUS_HIGH);
	for_each_set_bit(pin, &mis, 32)
		generic_handle_domain_irq(sfp->gc.irq.domain, pin + 32);

	chained_irq_exit(chip, desc);
}

static int starfive_gpio_init_hw(struct gpio_chip *gc)
{
	struct starfive_pinctrl *sfp = starfive_from_gc(gc);

	/* mask all GPIO interrupts */
	writel(0, sfp->base + IRQ_ENABLE_LOW);
	writel(0, sfp->base + IRQ_ENABLE_HIGH);
	/* clear edge interrupt flags */
	writel(~0U, sfp->base + IRQ_CLEAR_EDGE_LOW);
	writel(~0U, sfp->base + IRQ_CLEAR_EDGE_HIGH);
	/* enable GPIO interrupts */
	writel(1, sfp->base + IRQ_GLOBAL_EN);
	return 0;
}

#define MAX_GPI (GPI_USB_OVER_CURRENT + 1)
static void starfive_pinmux_reset(struct starfive_pinctrl *sfp)
{
	static const DECLARE_BITMAP(defaults, MAX_GPI) = {
		BIT_MASK(GPI_I2C0_PAD_SCK_IN) |
		BIT_MASK(GPI_I2C0_PAD_SDA_IN) |
		BIT_MASK(GPI_I2C1_PAD_SCK_IN) |
		BIT_MASK(GPI_I2C1_PAD_SDA_IN) |
		BIT_MASK(GPI_I2C2_PAD_SCK_IN) |
		BIT_MASK(GPI_I2C2_PAD_SDA_IN) |
		BIT_MASK(GPI_I2C3_PAD_SCK_IN) |
		BIT_MASK(GPI_I2C3_PAD_SDA_IN) |
		BIT_MASK(GPI_SDIO0_PAD_CARD_DETECT_N) |

		BIT_MASK(GPI_SDIO1_PAD_CARD_DETECT_N) |
		BIT_MASK(GPI_SPI0_PAD_SS_IN_N) |
		BIT_MASK(GPI_SPI1_PAD_SS_IN_N) |
		BIT_MASK(GPI_SPI2_PAD_SS_IN_N) |
		BIT_MASK(GPI_SPI2AHB_PAD_SS_N) |
		BIT_MASK(GPI_SPI3_PAD_SS_IN_N),

		BIT_MASK(GPI_UART0_PAD_SIN) |
		BIT_MASK(GPI_UART1_PAD_SIN) |
		BIT_MASK(GPI_UART2_PAD_SIN) |
		BIT_MASK(GPI_UART3_PAD_SIN) |
		BIT_MASK(GPI_USB_OVER_CURRENT)
	};
	DECLARE_BITMAP(keep, MAX_GPIO) = { 0 };
	const __be32 *list;
	int size = 0;
	int i;

	list = of_get_property(starfive_dev(sfp)->of_node, "starfive,keep-gpiomux", &size);
	for (i = 0; i < size; i += sizeof(*list)) {
		u32 gpio = be32_to_cpu(*list++);

		if (gpio < MAX_GPIO)
			set_bit(gpio, keep);
	}

	for (i = 0; i < MAX_GPIO; i++) {
		if (test_bit(i, keep))
			continue;

		writel_relaxed(GPO_DISABLE,
			       sfp->base + GPIO_N_DOEN_CFG + 8 * i);
		writel_relaxed(GPO_LOW,
			       sfp->base + GPIO_N_DOUT_CFG + 8 * i);
	}

	for (i = 0; i < MAX_GPI; i++) {
		void __iomem *reg = sfp->base + GPIO_IN_OFFSET + 4 * i;
		u32 din = readl_relaxed(reg);

		if (din >= 2 && din < (MAX_GPIO + 2) && test_bit(din - 2, keep))
			continue;

		writel_relaxed(test_bit(i, defaults), reg);
	}
}

static int __init starfive_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct starfive_pinctrl *sfp;
	u32 value;
	int ret;

	sfp = devm_kzalloc(dev, sizeof(*sfp), GFP_KERNEL);
	if (!sfp)
		return -ENOMEM;

	sfp->base = devm_platform_ioremap_resource_byname(pdev, "gpio");
	if (IS_ERR(sfp->base))
		return PTR_ERR(sfp->base);

	sfp->padctl = devm_platform_ioremap_resource_byname(pdev, "padctl");
	if (IS_ERR(sfp->padctl))
		return PTR_ERR(sfp->padctl);

	platform_set_drvdata(pdev, sfp);
	sfp->gc.parent = dev;
	raw_spin_lock_init(&sfp->lock);

	ret = devm_pinctrl_register_and_init(dev, &starfive_desc, sfp, &sfp->pctl);
	if (ret) {
		dev_err(dev, "could not register pinctrl driver: %d\n", ret);
		return ret;
	}

	if (!keepmux)
		starfive_pinmux_reset(sfp);

	if (!of_property_read_u32(dev->of_node, "starfive,signal-group", &value)) {
		if (value <= 6)
			writel(value, sfp->padctl + IO_PADSHARE_SEL);
		else
			dev_err(dev, "invalid signal group %u\n", value);
	}

	value = readl(sfp->padctl + IO_PADSHARE_SEL);
	switch (value) {
	case 0:
		sfp->gpios.pin_base = 0x10000;
		goto done;
	case 1:
		sfp->gpios.pin_base = PAD_GPIO(0);
		break;
	case 2:
		sfp->gpios.pin_base = PAD_FUNC_SHARE(72);
		break;
	case 3:
		sfp->gpios.pin_base = PAD_FUNC_SHARE(70);
		break;
	case 4: case 5: case 6:
		sfp->gpios.pin_base = PAD_FUNC_SHARE(0);
		break;
	default:
		dev_err(dev, "invalid signal group %u\n", value);
		return -EINVAL;
	}

	sfp->gc.label = dev_name(dev);
	sfp->gc.of_node = dev->of_node;
	sfp->gc.owner = THIS_MODULE;
	sfp->gc.request = starfive_gpio_request;
	sfp->gc.free = starfive_gpio_free;
	sfp->gc.get_direction = starfive_gpio_get_direction;
	sfp->gc.direction_input = starfive_gpio_direction_input;
	sfp->gc.direction_output = starfive_gpio_direction_output;
	sfp->gc.get = starfive_gpio_get;
	sfp->gc.set = starfive_gpio_set;
	sfp->gc.set_config = starfive_gpio_set_config;
	sfp->gc.add_pin_ranges = starfive_gpio_add_pin_ranges;
	sfp->gc.base = -1;
	sfp->gc.ngpio = MAX_GPIO;

	starfive_irq_chip.parent_device = dev;
	starfive_irq_chip.name = sfp->gc.label;

	sfp->gc.irq.chip = &starfive_irq_chip;
	sfp->gc.irq.parent_handler = starfive_gpio_irq_handler;
	sfp->gc.irq.parents =
		devm_kcalloc(dev, 1, sizeof(*sfp->gc.irq.parents), GFP_KERNEL);
	if (!sfp->gc.irq.parents)
		return -ENOMEM;
	sfp->gc.irq.num_parents = 1;
	sfp->gc.irq.default_type = IRQ_TYPE_NONE;
	sfp->gc.irq.handler = handle_bad_irq;
	sfp->gc.irq.init_hw = starfive_gpio_init_hw;

	ret = platform_get_irq(pdev, 0);
	if (ret < 0)
		return ret;
	sfp->gc.irq.parents[0] = ret;

	ret = devm_gpiochip_add_data(dev, &sfp->gc, sfp);
	if (ret) {
		dev_err(dev, "could not register gpiochip: %d\n", ret);
		return ret;
	}

	dev_info(dev, "StarFive GPIO chip registered %d GPIOs\n", sfp->gc.ngpio);
done:
	return pinctrl_enable(sfp->pctl);
}

static const struct of_device_id starfive_of_match[] = {
	{ .compatible = "starfive,jh7100-pinctrl" },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, starfive_of_match);

static struct platform_driver starfive_pinctrl_driver = {
	.probe = starfive_probe,
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = starfive_of_match,
	},
};
module_platform_driver(starfive_pinctrl_driver);

MODULE_AUTHOR("Emil Renner Berthing <kernel@esmil.dk>");
MODULE_DESCRIPTION("Pinctrl driver for the StarFive JH7100 SoC");
MODULE_LICENSE("GPL v2");
