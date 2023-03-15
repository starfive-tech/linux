// SPDX-License-Identifier: GPL-2.0+
/*
 * DPHY driver for the StarFive JH7110 SoC
 *
 * Copyright (C) 2023 StarFive Technology Co., Ltd.
 */

#include <linux/bitfield.h>
#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/reset.h>

#define STF_DPHY_APBCFGSAIF__SYSCFG(x)		(x)

#define STF_DPHY_DA_CDPHY_R100_CTRL0_2D1C_EFUSE_EN BIT(6)
#define STF_DPHY_DA_CDPHY_R100_CTRL0_2D1C_EFUSE_IN GENMASK(12, 7)
#define STF_DPHY_DA_CDPHY_R100_CTRL1_2D1C_EFUSE_EN BIT(19)
#define STF_DPHY_DA_CDPHY_R100_CTRL1_2D1C_EFUSE_IN GENMASK(25, 20)

#define STF_DPHY_DATA_BUS16_8			BIT(8)
#define STF_DPHY_DEBUG_MODE_SEL			GENMASK(15, 9)

#define STF_DPHY_ENABLE_CLK			BIT(6)
#define STF_DPHY_ENABLE_CLK1			BIT(7)
#define STF_DPHY_ENABLE_LAN0			BIT(8)
#define STF_DPHY_ENABLE_LAN1			BIT(9)
#define STF_DPHY_ENABLE_LAN2			BIT(10)
#define STF_DPHY_ENABLE_LAN3			BIT(11)
#define STF_DPHY_GPI_EN				GENMASK(17, 12)
#define STF_DPHY_HS_FREQ_CHANGE_CLK		BIT(18)
#define STF_DPHY_HS_FREQ_CHANGE_CLK1		BIT(19)
#define STF_DPHY_LANE_SWAP_CLK			GENMASK(22, 20)
#define STF_DPHY_LANE_SWAP_CLK1			GENMASK(25, 23)
#define STF_DPHY_LANE_SWAP_LAN0			GENMASK(28, 26)
#define STF_DPHY_LANE_SWAP_LAN1			GENMASK(31, 29)

#define STF_DPHY_LANE_SWAP_LAN2			GENMASK(2, 0)
#define STF_DPHY_LANE_SWAP_LAN3			GENMASK(5, 3)
#define STF_DPHY_MP_TEST_EN			BIT(6)
#define STF_DPHY_MP_TEST_MODE_SEL		GENMASK(11, 7)
#define STF_DPHY_PLL_CLK_SEL			GENMASK(21, 12)
#define STF_DPHY_PRECOUNTER_IN_CLK		GENMASK(29, 22)

#define STF_DPHY_PRECOUNTER_IN_CLK1		GENMASK(7, 0)
#define STF_DPHY_PRECOUNTER_IN_LAN0		GENMASK(15, 8)
#define STF_DPHY_PRECOUNTER_IN_LAN1		GENMASK(23, 16)
#define STF_DPHY_PRECOUNTER_IN_LAN2		GENMASK(31, 24)

#define STF_DPHY_PRECOUNTER_IN_LAN3		GENMASK(7, 0)
#define STF_DPHY_RX_1C2C_SEL			BIT(8)

struct regval_t {
	u32 addr;
	u32 val;
};

struct stf_dphy {
	struct device *dev;
	void __iomem *regs;
	struct clk *cfg_clk;
	struct clk *ref_clk;
	struct clk *tx_clk;
	struct reset_control *rstc;
	struct regulator *mipi_0p9;
	struct phy *phy;
	struct regmap *stf_aon_syscon;
	unsigned int aon_gp_reg;
};

struct stf_dphy_info {
	bool external_support;
	int (*external_get)(struct stf_dphy *dphy);
	void (*external_init)(struct stf_dphy *dphy);
	void (*external_exit)(struct stf_dphy *dphy);
};

static const struct stf_dphy_info *stf_dphy_info;

static const struct regval_t stf_dphy_init_list[] = {
	{ STF_DPHY_APBCFGSAIF__SYSCFG(4), 0x00000000 },
	{ STF_DPHY_APBCFGSAIF__SYSCFG(8), 0x00000000 },
	{ STF_DPHY_APBCFGSAIF__SYSCFG(12), 0x0000fff0 },
	{ STF_DPHY_APBCFGSAIF__SYSCFG(16), 0x00000000 },
	{ STF_DPHY_APBCFGSAIF__SYSCFG(20), 0x00000000 },
	{ STF_DPHY_APBCFGSAIF__SYSCFG(24), 0x00000000 },
	{ STF_DPHY_APBCFGSAIF__SYSCFG(28), 0x00000000 },
	{ STF_DPHY_APBCFGSAIF__SYSCFG(32), 0x00000000 },
	{ STF_DPHY_APBCFGSAIF__SYSCFG(36), 0x00000000 },
	{ STF_DPHY_APBCFGSAIF__SYSCFG(40), 0x00000000 },
	{ STF_DPHY_APBCFGSAIF__SYSCFG(40), 0x00000000 },
	{ STF_DPHY_APBCFGSAIF__SYSCFG(48), 0x24000000 },
	{ STF_DPHY_APBCFGSAIF__SYSCFG(52), 0x00000000 },
	{ STF_DPHY_APBCFGSAIF__SYSCFG(56), 0x00000000 },
	{ STF_DPHY_APBCFGSAIF__SYSCFG(60), 0x00000000 },
	{ STF_DPHY_APBCFGSAIF__SYSCFG(64), 0x00000000 },
	{ STF_DPHY_APBCFGSAIF__SYSCFG(68), 0x00000000 },
	{ STF_DPHY_APBCFGSAIF__SYSCFG(72), 0x00000000 },
	{ STF_DPHY_APBCFGSAIF__SYSCFG(76), 0x00000000 },
	{ STF_DPHY_APBCFGSAIF__SYSCFG(80), 0x00000000 },
	{ STF_DPHY_APBCFGSAIF__SYSCFG(84), 0x00000000 },
	{ STF_DPHY_APBCFGSAIF__SYSCFG(88), 0x00000000 },
	{ STF_DPHY_APBCFGSAIF__SYSCFG(92), 0x00000000 },
	{ STF_DPHY_APBCFGSAIF__SYSCFG(96), 0x00000000 },
	{ STF_DPHY_APBCFGSAIF__SYSCFG(100), 0x02000000 },
	{ STF_DPHY_APBCFGSAIF__SYSCFG(104), 0x00000000 },
	{ STF_DPHY_APBCFGSAIF__SYSCFG(108), 0x00000000 },
	{ STF_DPHY_APBCFGSAIF__SYSCFG(112), 0x00000000 },
	{ STF_DPHY_APBCFGSAIF__SYSCFG(116), 0x00000000 },
	{ STF_DPHY_APBCFGSAIF__SYSCFG(120), 0x00000000 },
	{ STF_DPHY_APBCFGSAIF__SYSCFG(124), 0x0000000c },
	{ STF_DPHY_APBCFGSAIF__SYSCFG(128), 0x00000000 },
	{ STF_DPHY_APBCFGSAIF__SYSCFG(132), 0xcc500000 },
	{ STF_DPHY_APBCFGSAIF__SYSCFG(136), 0x000000cc },
	{ STF_DPHY_APBCFGSAIF__SYSCFG(140), 0x00000000 },
	{ STF_DPHY_APBCFGSAIF__SYSCFG(144), 0x00000000 },
};

static int stf_dphy_configure(struct phy *phy, union phy_configure_opts *opts)
{
	struct stf_dphy *dphy = phy_get_drvdata(phy);
	int map[6] = {4, 0, 1, 2, 3, 5};
	int i;

	for (i = 0; i < ARRAY_SIZE(stf_dphy_init_list); i++)
		writel(stf_dphy_init_list[i].val,
		       dphy->regs + stf_dphy_init_list[i].addr);

	writel(FIELD_PREP(STF_DPHY_DA_CDPHY_R100_CTRL0_2D1C_EFUSE_EN, 1) |
	       FIELD_PREP(STF_DPHY_DA_CDPHY_R100_CTRL0_2D1C_EFUSE_IN, 0x1b) |
	       FIELD_PREP(STF_DPHY_DA_CDPHY_R100_CTRL1_2D1C_EFUSE_EN, 1) |
	       FIELD_PREP(STF_DPHY_DA_CDPHY_R100_CTRL1_2D1C_EFUSE_IN, 0x1b),
	       dphy->regs + STF_DPHY_APBCFGSAIF__SYSCFG(0));

	writel(FIELD_PREP(STF_DPHY_DATA_BUS16_8, 0) |
	       FIELD_PREP(STF_DPHY_DEBUG_MODE_SEL, 0x5a),
	       dphy->regs + STF_DPHY_APBCFGSAIF__SYSCFG(184));

	writel(FIELD_PREP(STF_DPHY_ENABLE_CLK, 1) |
	       FIELD_PREP(STF_DPHY_ENABLE_CLK1, 1) |
	       FIELD_PREP(STF_DPHY_ENABLE_LAN0, 1) |
	       FIELD_PREP(STF_DPHY_ENABLE_LAN1, 1) |
	       FIELD_PREP(STF_DPHY_ENABLE_LAN2, 1) |
	       FIELD_PREP(STF_DPHY_ENABLE_LAN3, 1) |
	       FIELD_PREP(STF_DPHY_GPI_EN, 0) |
	       FIELD_PREP(STF_DPHY_HS_FREQ_CHANGE_CLK, 0) |
	       FIELD_PREP(STF_DPHY_HS_FREQ_CHANGE_CLK1, 0) |
	       FIELD_PREP(STF_DPHY_LANE_SWAP_CLK, map[0]) |
	       FIELD_PREP(STF_DPHY_LANE_SWAP_CLK1, map[5]) |
	       FIELD_PREP(STF_DPHY_LANE_SWAP_LAN0, map[1]) |
	       FIELD_PREP(STF_DPHY_LANE_SWAP_LAN1, map[2]),
	       dphy->regs + STF_DPHY_APBCFGSAIF__SYSCFG(188));

	writel(FIELD_PREP(STF_DPHY_LANE_SWAP_LAN2, map[3]) |
	       FIELD_PREP(STF_DPHY_LANE_SWAP_LAN3, map[4]) |
	       FIELD_PREP(STF_DPHY_MP_TEST_EN, 0) |
	       FIELD_PREP(STF_DPHY_MP_TEST_MODE_SEL, 0) |
	       FIELD_PREP(STF_DPHY_PLL_CLK_SEL, 0x37c) |
	       FIELD_PREP(STF_DPHY_PRECOUNTER_IN_CLK, 8),
	       dphy->regs + STF_DPHY_APBCFGSAIF__SYSCFG(192));

	writel(FIELD_PREP(STF_DPHY_PRECOUNTER_IN_CLK1, 8) |
	       FIELD_PREP(STF_DPHY_PRECOUNTER_IN_LAN0, 7) |
	       FIELD_PREP(STF_DPHY_PRECOUNTER_IN_LAN1, 7) |
	       FIELD_PREP(STF_DPHY_PRECOUNTER_IN_LAN2, 7),
	       dphy->regs + STF_DPHY_APBCFGSAIF__SYSCFG(196));

	writel(FIELD_PREP(STF_DPHY_PRECOUNTER_IN_LAN3, 7) |
	       FIELD_PREP(STF_DPHY_RX_1C2C_SEL, 0),
	       dphy->regs + STF_DPHY_APBCFGSAIF__SYSCFG(200));

	return 0;
}

static int stf_dphy_init(struct phy *phy)
{
	struct stf_dphy *dphy = phy_get_drvdata(phy);
	int ret;

	ret = regulator_enable(dphy->mipi_0p9);
	if (ret)
		goto err_0p9;

	if (stf_dphy_info->external_support && stf_dphy_info->external_init)
		stf_dphy_info->external_init(dphy);

	return 0;

err_0p9:
	return ret;
}

static int stf_dphy_exit(struct phy *phy)
{
	struct stf_dphy *dphy = phy_get_drvdata(phy);

	if (stf_dphy_info->external_support && stf_dphy_info->external_exit)
		stf_dphy_info->external_exit(dphy);

	regulator_disable(dphy->mipi_0p9);

	return 0;
}

static int stf_dphy_power_on(struct phy *phy)
{
	struct stf_dphy *dphy = phy_get_drvdata(phy);

	clk_set_rate(dphy->cfg_clk, 99000000);
	clk_set_rate(dphy->ref_clk, 49500000);
	clk_set_rate(dphy->tx_clk, 19800000);
	reset_control_deassert(dphy->rstc);

	return 0;
}

static int stf_dphy_power_off(struct phy *phy)
{
	struct stf_dphy *dphy = phy_get_drvdata(phy);

	reset_control_assert(dphy->rstc);

	return 0;
}

static const struct phy_ops stf_dphy_ops = {
	.init      = stf_dphy_init,
	.exit      = stf_dphy_exit,
	.configure = stf_dphy_configure,
	.power_on  = stf_dphy_power_on,
	.power_off = stf_dphy_power_off,
};

static int stf_dphy_probe(struct platform_device *pdev)
{
	struct phy_provider *phy_provider;
	struct stf_dphy *dphy;
	int ret;

	dphy = devm_kzalloc(&pdev->dev, sizeof(*dphy), GFP_KERNEL);
	if (!dphy)
		return -ENOMEM;
	stf_dphy_info = of_device_get_match_data(&pdev->dev);
	dev_set_drvdata(&pdev->dev, dphy);
	dphy->dev = &pdev->dev;

	dphy->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(dphy->regs))
		return PTR_ERR(dphy->regs);

	dphy->cfg_clk = devm_clk_get(&pdev->dev, "cfg");
	if (IS_ERR(dphy->cfg_clk))
		return PTR_ERR(dphy->cfg_clk);

	dphy->ref_clk = devm_clk_get(&pdev->dev, "ref");
	if (IS_ERR(dphy->ref_clk))
		return PTR_ERR(dphy->ref_clk);

	dphy->tx_clk = devm_clk_get(&pdev->dev, "tx");
	if (IS_ERR(dphy->tx_clk))
		return PTR_ERR(dphy->tx_clk);

	dphy->rstc = devm_reset_control_array_get_exclusive(&pdev->dev);
	if (IS_ERR(dphy->rstc))
		return PTR_ERR(dphy->rstc);

	dphy->mipi_0p9 = devm_regulator_get(&pdev->dev, "mipi_0p9");
	if (IS_ERR(dphy->mipi_0p9))
		return PTR_ERR(dphy->mipi_0p9);

	if (stf_dphy_info->external_support && stf_dphy_info->external_get) {
		ret = stf_dphy_info->external_get(dphy);
		if (ret < 0) {
			dev_err(&pdev->dev, "Failed to get PHY external info\n");
			return ret;
		}
	}

	dphy->phy = devm_phy_create(&pdev->dev, NULL, &stf_dphy_ops);
	if (IS_ERR(dphy->phy)) {
		dev_err(&pdev->dev, "Failed to create PHY\n");
		return PTR_ERR(dphy->phy);
	}

	phy_set_drvdata(dphy->phy, dphy);
	phy_provider = devm_of_phy_provider_register(&pdev->dev,
						     of_phy_simple_xlate);

	return PTR_ERR_OR_ZERO(phy_provider);
}

static int stf_external_get(struct stf_dphy *dphy)
{
	struct of_phandle_args args;
	int ret;

	ret = of_parse_phandle_with_fixed_args(dphy->dev->of_node,
					       "starfive,aon-syscon",
					       1, 0, &args);
	if (ret < 0) {
		dev_err(dphy->dev, "Failed to parse starfive,aon-syscon\n");
		return -EINVAL;
	}

	dphy->stf_aon_syscon = syscon_node_to_regmap(args.np);
	of_node_put(args.np);
	if (IS_ERR(dphy->stf_aon_syscon))
		return PTR_ERR(dphy->stf_aon_syscon);

	dphy->aon_gp_reg = args.args[0];

	return 0;
}

static void stf_external_init(struct stf_dphy *dphy)
{
	regmap_update_bits(dphy->stf_aon_syscon, dphy->aon_gp_reg,
			   BIT(31), BIT(31));
}

static void stf_external_exit(struct stf_dphy *dphy)
{
	regmap_update_bits(dphy->stf_aon_syscon, dphy->aon_gp_reg,
			   BIT(31), 0);
}

static const struct stf_dphy_info starfive_dphy_info = {
	.external_support = true,
	.external_get = stf_external_get,
	.external_init = stf_external_init,
	.external_exit = stf_external_exit,
};

static const struct of_device_id stf_dphy_dt_ids[] = {
	{
		.compatible = "starfive,jh7110-dphy-rx",
		.data = &starfive_dphy_info,
	},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, stf_dphy_dt_ids);

static struct platform_driver stf_dphy_driver = {
	.probe = stf_dphy_probe,
	.driver = {
		.name	= "starfive-dphy-rx",
		.of_match_table = stf_dphy_dt_ids,
	},
};
module_platform_driver(stf_dphy_driver);

MODULE_AUTHOR("Jack Zhu <jack.zhu@starfivetech.com>");
MODULE_AUTHOR("Changhuang Liang <changhuang.liang@starfivetech.com>");
MODULE_DESCRIPTION("StarFive DPHY RX driver");
MODULE_LICENSE("GPL");
