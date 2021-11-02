/*
 * drivers/net/phy/motorcomm.c
 *
 * Driver for Motorcomm PHYs
 *
 * Author: Leilei Zhao <leilei.zhao@motorcomm.com>
 *
 * Copyright (c) 2019 Motorcomm, Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * Support : Motorcomm Phys:
 *		Giga phys: yt8511, yt8521
 *		100/10 Phys : yt8512, yt8512b, yt8510
 *		Automotive 100Mb Phys : yt8010
 *		Automotive 100/10 hyper range Phys: yt8510
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/phy.h>
#include <linux/motorcomm_phy.h>
#include <linux/of.h>
#include <linux/clk.h>
#ifndef LINUX_VERSION_CODE
#include <linux/version.h>
#else
#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))
#endif
/*for wol, 20210604*/
#include <linux/netdevice.h>

/**** configuration section begin ***********/
/* if system depends on ethernet packet to restore from sleep, please define this macro to 1
 * otherwise, define it to 0.
 */
#define SYS_WAKEUP_BASED_ON_ETH_PKT 	0

/* to enable system WOL of phy, please define this macro to 1
 * otherwise, define it to 0.
 */
#define YTPHY_ENABLE_WOL 		0

/* some GMAC need clock input from PHY, for eg., 125M, please enable this macro
 * by degault, it is set to 0
 * NOTE: this macro will need macro SYS_WAKEUP_BASED_ON_ETH_PKT to set to 1
 */
#define GMAC_CLOCK_INPUT_NEEDED 0

#if (YTPHY_ENABLE_WOL)
	#undef SYS_WAKEUP_BASED_ON_ETH_PKT
	#define SYS_WAKEUP_BASED_ON_ETH_PKT 	1
#endif

/**** configuration section end ***********/

#if ( LINUX_VERSION_CODE > KERNEL_VERSION(5,0,0) )
int genphy_config_init(struct phy_device *phydev)
{
	return  genphy_read_abilities(phydev);
}
#endif

static int ytphy_read_ext(struct phy_device *phydev, u32 regnum)
{
	int ret;
	int val;

	ret = phy_write(phydev, REG_DEBUG_ADDR_OFFSET, regnum);
	if (ret < 0)
		return ret;

	val = phy_read(phydev, REG_DEBUG_DATA);

	return val;
}

static int ytphy_write_ext(struct phy_device *phydev, u32 regnum, u16 val)
{
	int ret;

	ret = phy_write(phydev, REG_DEBUG_ADDR_OFFSET, regnum);
	if (ret < 0)
		return ret;

	ret = phy_write(phydev, REG_DEBUG_DATA, val);

	return ret;
}

/* qingsong feedback 2 genphy_soft_reset will cause problem.
 * and this is the reduction version
 */
int yt8521_soft_reset(struct phy_device *phydev)
{
	int ret, val;

	val = ytphy_read_ext(phydev, 0xa001);
	ytphy_write_ext(phydev, 0xa001, (val & ~0x8000));

	ret = genphy_soft_reset(phydev);
	if (ret < 0)
		return ret;

	return 0;
}

#if GMAC_CLOCK_INPUT_NEEDED
static int ytphy_mii_rd_ext(struct mii_bus *bus, int phy_id, u32 regnum)
{
	int ret;
	int val;

	ret = bus->write(bus, phy_id, REG_DEBUG_ADDR_OFFSET, regnum);
	if (ret < 0)
		return ret;

	val = bus->read(bus, phy_id, REG_DEBUG_DATA);

	return val;
}

static int ytphy_mii_wr_ext(struct mii_bus *bus, int phy_id, u32 regnum, u16 val)
{
	int ret;

	ret = bus->write(bus, phy_id, REG_DEBUG_ADDR_OFFSET, regnum);
	if (ret < 0)
		return ret;

	ret = bus->write(bus, phy_id, REG_DEBUG_DATA, val);

	return ret;
}

int yt8511_config_dis_txdelay(struct mii_bus *bus, int phy_id)
{
    int ret;
    int val;

    /* disable auto sleep */
    val = ytphy_mii_rd_ext(bus, phy_id, 0x27);
    if (val < 0)
            return val;

    val &= (~BIT(15));

    ret = ytphy_mii_wr_ext(bus, phy_id, 0x27, val);
    if (ret < 0)
            return ret;

    /* enable RXC clock when no wire plug */
    val = ytphy_mii_rd_ext(bus, phy_id, 0xc);
    if (val < 0)
            return val;

    /* ext reg 0xc b[7:4]
	Tx Delay time = 150ps * N - 250ps
    */
    val &= ~(0xf << 4);
    ret = ytphy_mii_wr_ext(bus, phy_id, 0xc, val);
    printk("yt8511_config_dis_txdelay..phy txdelay, val=%#08x\n",val);

    return ret;
}


int yt8511_config_out_125m(struct mii_bus *bus, int phy_id)
{
	int ret;
	int val;

	/* disable auto sleep */
	val = ytphy_mii_rd_ext(bus, phy_id, 0x27);
	if (val < 0)
	        return val;

	val &= (~BIT(15));

	ret = ytphy_mii_wr_ext(bus, phy_id, 0x27, val);
	if (ret < 0)
	        return ret;

	/* enable RXC clock when no wire plug */
	val = ytphy_mii_rd_ext(bus, phy_id, 0xc);
	if (val < 0)
	        return val;

	/* ext reg 0xc.b[2:1]
	00-----25M from pll;
	01---- 25M from xtl;(default)
	10-----62.5M from pll;
	11----125M from pll(here set to this value)
	*/
	val |= (3 << 1);
	ret = ytphy_mii_wr_ext(bus, phy_id, 0xc, val);
	printk("yt8511_config_out_125m, phy clk out, val=%#08x\n",val);

#if 0
	/* for customer, please enable it based on demand.
	 * configure to master
	 */	
	val = bus->read(bus, phy_id, 0x9/*master/slave config reg*/);
	val |= (0x3<<11); //to be manual config and force to be master
	ret = bus->write(bus, phy_id, 0x9, val); //take effect until phy soft reset
	if (ret < 0)
		return ret;

	printk("yt8511_config_out_125m, phy to be master, val=%#08x\n",val);
#endif

    return ret;
}
#endif /*GMAC_CLOCK_INPUT_NEEDED*/

#if (YTPHY_ENABLE_WOL)
static int ytphy_switch_reg_space(struct phy_device *phydev, int space)
{
	int ret;

	if (space == YTPHY_REG_SPACE_UTP){
		ret = ytphy_write_ext(phydev, 0xa000, 0);
	}else{
		ret = ytphy_write_ext(phydev, 0xa000, 2);
	}
	
	return ret;
}

static int ytphy_wol_en_cfg(struct phy_device *phydev, ytphy_wol_cfg_t wol_cfg)
{
	int ret=0;
	int val=0;

	val = ytphy_read_ext(phydev, YTPHY_WOL_CFG_REG);
	if (val < 0)
		return val;

	if(wol_cfg.enable) {
		val |= YTPHY_WOL_CFG_EN;

		if(wol_cfg.type == YTPHY_WOL_TYPE_LEVEL) {
			val &= ~YTPHY_WOL_CFG_TYPE;
			val &= ~YTPHY_WOL_CFG_INTR_SEL;
		} else if(wol_cfg.type == YTPHY_WOL_TYPE_PULSE) {
			val |= YTPHY_WOL_CFG_TYPE;
			val |= YTPHY_WOL_CFG_INTR_SEL;

			if(wol_cfg.width == YTPHY_WOL_WIDTH_84MS) {
				val &= ~YTPHY_WOL_CFG_WIDTH1;
				val &= ~YTPHY_WOL_CFG_WIDTH2;
			} else if(wol_cfg.width == YTPHY_WOL_WIDTH_168MS) {
				val |= YTPHY_WOL_CFG_WIDTH1;
				val &= ~YTPHY_WOL_CFG_WIDTH2;
			} else if(wol_cfg.width == YTPHY_WOL_WIDTH_336MS) {
				val &= ~YTPHY_WOL_CFG_WIDTH1;
				val |= YTPHY_WOL_CFG_WIDTH2;
			} else if(wol_cfg.width == YTPHY_WOL_WIDTH_672MS) {
				val |= YTPHY_WOL_CFG_WIDTH1;
				val |= YTPHY_WOL_CFG_WIDTH2;
			}
		}
	} else {
		val &= ~YTPHY_WOL_CFG_EN;
		val &= ~YTPHY_WOL_CFG_INTR_SEL;
	}

	ret = ytphy_write_ext(phydev, YTPHY_WOL_CFG_REG, val);
	if (ret < 0)
		return ret;

	return 0;
}

static void ytphy_get_wol(struct phy_device *phydev, struct ethtool_wolinfo *wol)
{
	int val = 0;

	wol->supported = WAKE_MAGIC;
	wol->wolopts = 0;

	val = ytphy_read_ext(phydev, YTPHY_WOL_CFG_REG);
	if (val < 0)
		return;

	if (val & YTPHY_WOL_CFG_EN)
		wol->wolopts |= WAKE_MAGIC;

	return;
}

static int ytphy_set_wol(struct phy_device *phydev, struct ethtool_wolinfo *wol)
{
	int ret, pre_page, val;
	ytphy_wol_cfg_t wol_cfg;
	struct net_device *p_attached_dev = phydev->attached_dev;

	memset(&wol_cfg,0,sizeof(ytphy_wol_cfg_t));
	pre_page = ytphy_read_ext(phydev, 0xa000);
	if (pre_page < 0)
		return pre_page;

	/* Switch to phy UTP page */
	ret = ytphy_switch_reg_space(phydev, YTPHY_REG_SPACE_UTP);
	if (ret < 0)
		return ret;

	if (wol->wolopts & WAKE_MAGIC) {
		
		/* Enable the WOL interrupt */
		val = phy_read(phydev, YTPHY_UTP_INTR_REG);
		val |= YTPHY_WOL_INTR;
		ret = phy_write(phydev, YTPHY_UTP_INTR_REG, val);
		if (ret < 0)
			return ret;

		/* Set the WOL config */
		wol_cfg.enable = 1; //enable
		wol_cfg.type= YTPHY_WOL_TYPE_PULSE;
		wol_cfg.width= YTPHY_WOL_WIDTH_672MS;
		ret = ytphy_wol_en_cfg(phydev, wol_cfg);
		if (ret < 0)
			return ret;

		/* Store the device address for the magic packet */
		ret = ytphy_write_ext(phydev, YTPHY_MAGIC_PACKET_MAC_ADDR2,
				((p_attached_dev->dev_addr[0] << 8) |
				 p_attached_dev->dev_addr[1]));
		if (ret < 0)
			return ret;
		ret = ytphy_write_ext(phydev, YTPHY_MAGIC_PACKET_MAC_ADDR1,
				((p_attached_dev->dev_addr[2] << 8) |
				 p_attached_dev->dev_addr[3]));
		if (ret < 0)
			return ret;
		ret = ytphy_write_ext(phydev, YTPHY_MAGIC_PACKET_MAC_ADDR0,
				((p_attached_dev->dev_addr[4] << 8) |
				 p_attached_dev->dev_addr[5]));
		if (ret < 0)
			return ret;
	} else {
		wol_cfg.enable = 0; //disable
		wol_cfg.type= YTPHY_WOL_TYPE_MAX;
		wol_cfg.width= YTPHY_WOL_WIDTH_MAX;
		ret = ytphy_wol_en_cfg(phydev, wol_cfg);
		if (ret < 0)
			return ret;
	}

	/* Recover to previous register space page */
	ret = ytphy_switch_reg_space(phydev, pre_page);
	if (ret < 0)
		return ret;

	return 0;
}
#endif /*(YTPHY_ENABLE_WOL)*/

static int yt8521_config_init(struct phy_device *phydev)
{
	int ret;
	int val;

	phydev->irq = PHY_POLL;

	ytphy_write_ext(phydev, 0xa000, 0);

	ret = genphy_config_init(phydev);
	if (ret < 0)
		return ret;

	/* disable auto sleep */
	val = ytphy_read_ext(phydev, YT8521_EXTREG_SLEEP_CONTROL1);
	if (val < 0)
		return val;

	val &= (~BIT(YT8521_EN_SLEEP_SW_BIT));

	ret = ytphy_write_ext(phydev, YT8521_EXTREG_SLEEP_CONTROL1, val);
	if (ret < 0)
		return ret;

	/*  enable tx delay 450ps per step */
	val = ytphy_read_ext(phydev, 0xa003);
	if (val < 0) {
		printk(KERN_INFO "yt8521_config: read 0xa003 error!\n");
		return val;
	}
	val |= 0x3; 
	ret = ytphy_write_ext(phydev, 0xa003, val);
	if (ret < 0) {
		printk(KERN_INFO "yt8521_config: set 0xa003 error!\n");
		return ret;
	}

	/* disable rx delay */
		printk(KERN_INFO ">>>>>>>>yt8521_config: disable rx delay\n");
	val = ytphy_read_ext(phydev, 0xa001);
	if (val < 0) {
		printk(KERN_INFO "yt8521_config: read 0xa001 error!\n");
		return val;
	}
	val &= ~(1<<8);
	ret = ytphy_write_ext(phydev, 0xa001, val);
	if (ret < 0) {
		printk(KERN_INFO "yt8521_config: failed to disable rx_delay!\n");
		return ret;
	}
	
	/* enable RXC clock when no wire plug */
	ret = ytphy_write_ext(phydev, 0xa000, 0);
	if (ret < 0)
		return ret;

	val = ytphy_read_ext(phydev, 0xc);
	if (val < 0)
		return val;
	val &= ~(1 << 12);
	ret = ytphy_write_ext(phydev, 0xc, val);
	if (ret < 0)
		return ret;

	printk(KERN_INFO "yt8521_config_init, 8521 init call out.\n");
	return ret;
}

/*
 * for fiber mode, there is no 10M speed mode and 
 * this function is for this purpose.
 */
static int yt8521_adjust_status(struct phy_device *phydev, int val, int is_utp)
{
	int speed_mode, duplex;
	int speed = SPEED_UNKNOWN;

	duplex = (val & YT8512_DUPLEX) >> YT8521_DUPLEX_BIT;
	speed_mode = (val & YT8521_SPEED_MODE) >> YT8521_SPEED_MODE_BIT;
	switch (speed_mode) {
	case 0:
		if (is_utp)
			speed = SPEED_10;
		break;
	case 1:
		speed = SPEED_100;
		break;
	case 2:
		speed = SPEED_1000;
		break;
	case 3:
		break;
	default:
		speed = SPEED_UNKNOWN;
		break;
	}

	phydev->speed = speed;
	phydev->duplex = duplex;
	//printk (KERN_INFO "yt8521_adjust_status call out,regval=0x%04x,mode=%s,speed=%dm...\n", val,is_utp?"utp":"fiber", phydev->speed);

	return 0;
}

int yt8521_aneg_done(struct phy_device *phydev)
{
	return genphy_aneg_done(phydev);
}

static int yt8521_read_status(struct phy_device *phydev)
{
	int ret;
	volatile int val;
	volatile int link;
	int link_utp = 0;

	/* reading UTP */
	ret = ytphy_write_ext(phydev, 0xa000, 0);
	if (ret < 0)
		return ret;

	val = phy_read(phydev, REG_PHY_SPEC_STATUS);
	if (val < 0)
		return val;

	link = val & (BIT(YT8521_LINK_STATUS_BIT));
	if (link) {
		link_utp = 1;
		yt8521_adjust_status(phydev, val, 1);
	} else {
		link_utp = 0;
	}

	if (link_utp) {
		phydev->link = 1;
		ytphy_write_ext(phydev, 0xa000, 0);
	} else {
		phydev->link = 0;
	}

	return 0;
}

int yt8521_suspend(struct phy_device *phydev)
{				
#if !(SYS_WAKEUP_BASED_ON_ETH_PKT)				
	int value;

	ytphy_write_ext(phydev, 0xa000, 0);
	value = phy_read(phydev, MII_BMCR);
	phy_write(phydev, MII_BMCR, value | BMCR_PDOWN);

	ytphy_write_ext(phydev, 0xa000, 2);
	value = phy_read(phydev, MII_BMCR);
	phy_write(phydev, MII_BMCR, value | BMCR_PDOWN);

	ytphy_write_ext(phydev, 0xa000, 0);
#endif /*!(SYS_WAKEUP_BASED_ON_ETH_PKT)*/

	return 0;
}

int yt8521_resume(struct phy_device *phydev)
{			
#if !(SYS_WAKEUP_BASED_ON_ETH_PKT)				
	int value;
	int ret;
	
	ytphy_write_ext(phydev, 0xa000, 0);
	value = phy_read(phydev, MII_BMCR);
	phy_write(phydev, MII_BMCR, value & ~BMCR_PDOWN);

	/* disable auto sleep */
	value = ytphy_read_ext(phydev, YT8521_EXTREG_SLEEP_CONTROL1);
	if (value < 0)
		return value;

	value &= (~BIT(YT8521_EN_SLEEP_SW_BIT));
	ret = ytphy_write_ext(phydev, YT8521_EXTREG_SLEEP_CONTROL1, value);
	if (ret < 0)
		return ret;

	/* enable RXC clock when no wire plug */
	value = ytphy_read_ext(phydev, 0xc);
	if (value < 0)
		return value;
	value &= ~(1 << 12);
	ret = ytphy_write_ext(phydev, 0xc, value);
	if (ret < 0)
		return ret;

	ytphy_write_ext(phydev, 0xa000, 2);
	value = phy_read(phydev, MII_BMCR);
	phy_write(phydev, MII_BMCR, value & ~BMCR_PDOWN);

	ytphy_write_ext(phydev, 0xa000, 0);

#endif /*!(SYS_WAKEUP_BASED_ON_ETH_PKT)*/

	return 0;
}

static struct phy_driver ytphy_drvs[] = {
	{
		.phy_id		= PHY_ID_YT8511,
		.name		= "YT8511 Gigabit Ethernet",
		.phy_id_mask	= MOTORCOMM_PHY_ID_MASK,
		.config_aneg	= genphy_config_aneg,
		.config_init	= genphy_config_init,
		.read_status	= genphy_read_status,
		.suspend	= genphy_suspend,
		.resume		= genphy_resume,
	}, 
	{
		.phy_id 	= PHY_ID_YT8521,
		.name 		= "YT8521 Ethernet",
		.phy_id_mask 	= MOTORCOMM_PHY_ID_MASK,
		.flags 		= PHY_POLL,
		.soft_reset	= yt8521_soft_reset,
		.config_aneg 	= genphy_config_aneg,
		.aneg_done	= yt8521_aneg_done,
		.config_init 	= yt8521_config_init,
		.read_status 	= yt8521_read_status,
		.suspend 	= yt8521_suspend,
		.resume 	= yt8521_resume,               
#if (YTPHY_ENABLE_WOL)
		.get_wol	= &ytphy_get_wol,
		.set_wol	= &ytphy_set_wol,
#endif
	},
};

module_phy_driver(ytphy_drvs);

MODULE_DESCRIPTION("Motorcomm PHY driver");
MODULE_AUTHOR("Leilei Zhao");
MODULE_LICENSE("GPL");

static struct mdio_device_id __maybe_unused motorcomm_tbl[] = {
	{ PHY_ID_YT8511, MOTORCOMM_PHY_ID_MASK },
	{ PHY_ID_YT8521, MOTORCOMM_PHY_ID_MASK },
	{ }
};

MODULE_DEVICE_TABLE(mdio, motorcomm_tbl);

