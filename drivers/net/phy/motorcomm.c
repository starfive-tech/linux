// SPDX-License-Identifier: GPL-2.0+
/*
 * Driver for Motorcomm PHYs
 *
 * Author: Peter Geis <pgwipeout@gmail.com>
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/phy.h>

#define PHY_ID_YT8511		0x0000010a
#define PHY_ID_YT8521		0x0000011a

#define YT8511_PAGE_SELECT	0x1e
#define YT8511_PAGE		0x1f
#define YT8511_EXT_CLK_GATE	0x0c
#define YT8511_EXT_DELAY_DRIVE	0x0d
#define YT8511_EXT_SLEEP_CTRL	0x27

#define YT8521_EXT_SMI_SDS_PHY		0xa000
#define YT8521_EXT_CHIP_CONFIG		0xa001
#define YT8521_EXT_RGMII_CONFIG1	0xa003

/* 2b00 25m from pll
 * 2b01 25m from xtl *default*
 * 2b10 62.m from pll
 * 2b11 125m from pll
 */
#define YT8511_CLK_125M		(BIT(2) | BIT(1))
#define YT8511_PLLON_SLP	BIT(14)

/* RX Delay enabled = 1.8ns 1000T, 8ns 10/100T */
#define YT8511_DELAY_RX		BIT(0)

/* TX Gig-E Delay is bits 7:4, default 0x5
 * TX Fast-E Delay is bits 15:12, default 0xf
 * Delay = 150ps * N - 250ps
 * On = 2000ps, off = 50ps
 */
#define YT8511_DELAY_GE_TX_EN	(0xf << 4)
#define YT8511_DELAY_GE_TX_DIS	(0x2 << 4)
#define YT8511_DELAY_FE_TX_EN	(0xf << 12)
#define YT8511_DELAY_FE_TX_DIS	(0x2 << 12)

/* YT8521 SMI_SDS_PHY */
#define YT8521_SMI_SDS_PHY_UTP	0
#define YT8521_SMI_SDS_PHY_SDS	BIT(1)

/* YT8521 Chip_Config */
#define YT8521_SW_RST_N_MODE	BIT(15)
#define YT8521_RXC_DLY_EN	BIT(8)

/* YT8521 RGMII_Config1 */
#define YT8521_TX_DELAY_SEL	GENMASK(3, 0)

static int yt8511_read_page(struct phy_device *phydev)
{
	return __phy_read(phydev, YT8511_PAGE_SELECT);
};

static int yt8511_write_page(struct phy_device *phydev, int page)
{
	return __phy_write(phydev, YT8511_PAGE_SELECT, page);
};

static int yt8511_config_init(struct phy_device *phydev)
{
	int oldpage, ret = 0;
	unsigned int ge, fe;

	oldpage = phy_select_page(phydev, YT8511_EXT_CLK_GATE);
	if (oldpage < 0)
		goto err_restore_page;

	/* set rgmii delay mode */
	switch (phydev->interface) {
	case PHY_INTERFACE_MODE_RGMII:
		ge = YT8511_DELAY_GE_TX_DIS;
		fe = YT8511_DELAY_FE_TX_DIS;
		break;
	case PHY_INTERFACE_MODE_RGMII_RXID:
		ge = YT8511_DELAY_RX | YT8511_DELAY_GE_TX_DIS;
		fe = YT8511_DELAY_FE_TX_DIS;
		break;
	case PHY_INTERFACE_MODE_RGMII_TXID:
		ge = YT8511_DELAY_GE_TX_EN;
		fe = YT8511_DELAY_FE_TX_EN;
		break;
	case PHY_INTERFACE_MODE_RGMII_ID:
		ge = YT8511_DELAY_RX | YT8511_DELAY_GE_TX_EN;
		fe = YT8511_DELAY_FE_TX_EN;
		break;
	default: /* do not support other modes */
		ret = -EOPNOTSUPP;
		goto err_restore_page;
	}

	ret = __phy_modify(phydev, YT8511_PAGE, (YT8511_DELAY_RX | YT8511_DELAY_GE_TX_EN), ge);
	if (ret < 0)
		goto err_restore_page;

	/* set clock mode to 125mhz */
	ret = __phy_modify(phydev, YT8511_PAGE, 0, YT8511_CLK_125M);
	if (ret < 0)
		goto err_restore_page;

	/* fast ethernet delay is in a separate page */
	ret = __phy_write(phydev, YT8511_PAGE_SELECT, YT8511_EXT_DELAY_DRIVE);
	if (ret < 0)
		goto err_restore_page;

	ret = __phy_modify(phydev, YT8511_PAGE, YT8511_DELAY_FE_TX_EN, fe);
	if (ret < 0)
		goto err_restore_page;

	/* leave pll enabled in sleep */
	ret = __phy_write(phydev, YT8511_PAGE_SELECT, YT8511_EXT_SLEEP_CTRL);
	if (ret < 0)
		goto err_restore_page;

	ret = __phy_modify(phydev, YT8511_PAGE, 0, YT8511_PLLON_SLP);
	if (ret < 0)
		goto err_restore_page;

err_restore_page:
	return phy_restore_page(phydev, oldpage, ret);
}

static int yt8521_soft_reset(struct phy_device *phydev)
{
	phy_modify_paged(phydev, YT8521_EXT_CHIP_CONFIG, YT8511_PAGE,
			 YT8521_SW_RST_N_MODE, 0);

	return genphy_soft_reset(phydev);
}

static int yt8521_config_init(struct phy_device *phydev)
{
	int oldpage, ret = 0;

	oldpage = phy_select_page(phydev, YT8521_EXT_SMI_SDS_PHY);
	if (oldpage < 0)
		goto err_restore_page;

	/* switch to UTP access */
	ret = __phy_write(phydev, YT8511_PAGE, YT8521_SMI_SDS_PHY_UTP);
	if (ret < 0)
		goto err_restore_page;

	/* set tx delay to 3 * 150ps */
	ret = __phy_write(phydev, YT8511_PAGE_SELECT, YT8521_EXT_RGMII_CONFIG1);
	if (ret < 0)
		goto err_restore_page;
	ret = __phy_modify(phydev, YT8511_PAGE, YT8521_TX_DELAY_SEL, 3);
	if (ret < 0)
		goto err_restore_page;

	/* disable rx delay */
	ret = __phy_write(phydev, YT8511_PAGE_SELECT, YT8521_EXT_CHIP_CONFIG);
	if (ret < 0)
		goto err_restore_page;
	ret = __phy_modify(phydev, YT8511_PAGE, YT8521_RXC_DLY_EN, 0);
	if (ret < 0)
		goto err_restore_page;

err_restore_page:
	return phy_restore_page(phydev, oldpage, ret);
}

static struct phy_driver motorcomm_phy_drvs[] = {
	{
		PHY_ID_MATCH_EXACT(PHY_ID_YT8511),
		.name		= "YT8511 Gigabit Ethernet",
		.config_init	= yt8511_config_init,
		.suspend	= genphy_suspend,
		.resume		= genphy_resume,
		.read_page	= yt8511_read_page,
		.write_page	= yt8511_write_page,
	},
	{
		PHY_ID_MATCH_EXACT(PHY_ID_YT8521),
		.name		= "YT8521 Gigabit Ethernet",
		.soft_reset	= yt8521_soft_reset,
		.config_init	= yt8521_config_init,
		.suspend	= genphy_suspend,
		.resume		= genphy_resume,
		.read_page	= yt8511_read_page,
		.write_page	= yt8511_write_page,
	},
};

module_phy_driver(motorcomm_phy_drvs);

MODULE_DESCRIPTION("Motorcomm PHY driver");
MODULE_AUTHOR("Peter Geis");
MODULE_LICENSE("GPL");

static const struct mdio_device_id __maybe_unused motorcomm_tbl[] = {
	{ PHY_ID_MATCH_EXACT(PHY_ID_YT8511) },
	{ PHY_ID_MATCH_EXACT(PHY_ID_YT8521) },
	{ /* sentinal */ }
};

MODULE_DEVICE_TABLE(mdio, motorcomm_tbl);
