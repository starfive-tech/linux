/*
 * StarFive Vout driver
 *
 * Copyright 2021 StarFive Inc.
 *
 * Licensed under the GPL-2.
 */

#ifndef __SF_FB_MIPI_TX_H__
#define __SF_FB_MIPI_TX_H__

//PHY timing paramter
#define TVG_MODE 0x2

#define	BIT_RATE 1500

#define DSI_RATE ((BIT_RATE/100)+1)*100
#define DPHY_REG_WAKEUP_TIME  0xE5  //0x27


#define DSI_CONN_LCDC 0
#define CSI_CONN_LCDC 1

/* DSI PPI Layer Registers */
#define PPI_STARTPPI        0x0104
#define PPI_BUSYPPI     0x0108
#define PPI_LINEINITCNT     0x0110
#define PPI_LPTXTIMECNT     0x0114
#define PPI_CLS_ATMR        0x0140
#define PPI_D0S_ATMR        0x0144
#define PPI_D1S_ATMR        0x0148
#define PPI_D0S_CLRSIPOCOUNT    0x0164
#define PPI_D1S_CLRSIPOCOUNT    0x0168
#define CLS_PRE         0x0180
#define D0S_PRE         0x0184
#define D1S_PRE         0x0188
#define CLS_PREP        0x01A0
#define D0S_PREP        0x01A4
#define D1S_PREP        0x01A8
#define CLS_ZERO        0x01C0
#define D0S_ZERO        0x01C4
#define D1S_ZERO        0x01C8
#define PPI_CLRFLG      0x01E0
#define PPI_CLRSIPO     0x01E4
#define HSTIMEOUT       0x01F0
#define HSTIMEOUTENABLE     0x01F4

/* DSI Protocol Layer Registers */
#define DSI_STARTDSI        0x0204
#define DSI_BUSYDSI     0x0208
#define DSI_LANEENABLE      0x0210
# define DSI_LANEENABLE_CLOCK       BIT(0)
# define DSI_LANEENABLE_D0      BIT(1)
# define DSI_LANEENABLE_D1      BIT(2)

#define DSI_LANESTATUS0     0x0214
#define DSI_LANESTATUS1     0x0218
#define DSI_INTSTATUS       0x0220
#define DSI_INTMASK     0x0224
#define DSI_INTCLR      0x0228
#define DSI_LPTXTO      0x0230
#define DSI_MODE        0x0260
#define DSI_PAYLOAD0        0x0268
#define DSI_PAYLOAD1        0x026C
#define DSI_SHORTPKTDAT     0x0270
#define DSI_SHORTPKTREQ     0x0274
#define DSI_BTASTA      0x0278
#define DSI_BTACLR      0x027C

/* LCDC/DPI Host Registers */
#define LCDCTRL         0x0420
#define HSR         0x0424
#define HDISPR          0x0428
#define VSR         0x042C
#define VDISPR          0x0430
#define VFUEN           0x0434

/* DBI-B Host Registers */
#define DBIBCTRL        0x0440

/* SPI Master Registers */
#define SPICMR          0x0450
#define SPITCR          0x0454

/* System Controller Registers */
#define SYSSTAT         0x0460
#define SYSCTRL         0x0464
#define SYSPLL1         0x0468
#define SYSPLL2         0x046C
#define SYSPLL3         0x0470
#define SYSPMCTRL       0x047C

/*mipi cmd*/
#define CMD_HEAD_WRITE_0    0x05
#define CMD_HEAD_WRITE_1    0x15
#define CMD_HEAD_WRITE_N    0x39
#define CMD_HEAD_READ       0x06

#define CMD_NAT_WRITE       0x00
#define CMD_NAT_READ        0x01
#define CMD_NAT_TE          0x04
#define CMD_NAT_TRIGGER     0x05
#define CMD_NAT_BTA         0x06

//dsitx reg , base addr 0x12100000
#define  MAIN_DATA_CTRL_ADDR  0x004
#define  MAIN_EN_ADDR         0x00C
#define  MAIN_PHY_CTRL_ADDR   0x08
#define  MAIN_STAT_CTRL_ADDR  0x130
#define  MAIN_STAT_ADDR       0x024
#define  MAIN_STAT_CLR_ADDR   0x150
#define  MAIN_STAT_FLAG_ADDR  0x170

#define  PHY_CTRL_ADDR        0x008
#define  PHY_TIMEOUT1_ADDR    0x014
#define  PHY_TIMEOUT2_ADDR    0x018
#define  ULPOUT_TIME_ADDR     0x01c
#define  DPHY_ERR_ADDR        0x28
#define  LANE_STAT_ADDR       0x2C
#define  PHY_SKEWCAL_TIMEOUT_ADDR 0x040
#define  PHY_ERR_CTRL1_ADDR   0x148
#define  PHY_ERR_CTRL2_ADDR   0x14c
#define  PHY_ERR_CLR_ADDR     0x168
#define  PHY_ERR_FLAG_ADDR    0x188

#define  CMD_MODE_CTRL_ADDR   0x70
#define  CMD_MODE_CTRL2_ADDR  0x74
#define  CMD_MODE_STAT_ADDR   0x78
#define  CMD_MODE_STAT_CTRL_ADDR     0x134
#define  CMD_MODE_STAT_CLR_ADDR      0x154
#define  CMD_MODE_STAT_FLAG_ADDR     0x174

#define  DIRECT_CMD_STAT_CTRL_ADDR   0x138
#define  DIRECT_CMD_STAT_CTL_ADDR    0x140

#define  DIRECT_CMD_STAT_CLR_ADDR    0x158
#define  DIRECT_CMD_STAT_FLAG_ADDR   0x178
#define  DIRECT_CMD_RDSTAT_CTRL_ADDR 0x13C
#define  DIRECT_CMD_RDSTAT_CLR_ADDR  0x15C
#define  DIRECT_CMD_RDSTAT_FLAG_ADDR 0x17C
#define  DIRECT_CMD_SEND_ADDR    0x80
#define  DIRECT_CMD_MAINSET_ADDR 0x84
#define  DIRECT_CMD_STAT_ADDR    0x88
#define  DIRECT_CMD_RDINIT_ADDR  0x8c
#define  DIRECT_CMD_WRDAT_ADDR   0x90
#define  DIRECT_CMD_FIFORST_ADDR 0x94
#define  DIRECT_CMD_RDATA_ADDR  0xa0
#define  DIRECT_CMD_RDPROP_ADDR 0xa4
#define  DIRECT_CMD_RDSTAT_ADDR 0xa8

//dsitx registers
#define  VID_MCTL_MAIN_DATA_CTL	        0x04
#define  VID_MCTL_MAIN_PHY_CTL	        0x08
#define  VID_MCTL_MAIN_EN	            0x0c
#define  VID_MAIN_CTRL_ADDR    0xb0
#define  VID_VSIZE1_ADDR       0xb4
#define  VID_VSIZE2_ADDR       0xb8
#define  VID_HSIZE1_ADDR       0xc0
#define  VID_HSIZE2_ADDR       0xc4
#define  VID_BLKSIZE1_ADDR     0xCC
#define  VID_BLKSIZE2_ADDR     0xd0
#define  VID_PCK_TIME_ADDR     0xd8
#define  VID_DPHY_TIME_ADDR    0xdc
#define  VID_ERR_COLOR1_ADDR   0xe0
#define  VID_ERR_COLOR2_ADDR   0xe4
#define  VID_VPOS_ADDR         0xe8
#define  VID_HPOS_ADDR         0xec
#define  VID_MODE_STAT_ADDR    0xf0
#define  VID_VCA_SET1_ADDR     0xf4
#define  VID_VCA_SET2_ADDR     0xf8


#define  VID_MODE_STAT_CLR_ADDR    0x160
#define  VID_MODE_STAT_FLAG_ADDR   0x180

#define  TVG_CTRL_ADDR      0x0fc
#define  TVG_IMG_SIZE_ADDR  0x100
#define  TVG_COLOR1_ADDR    0x104
#define  TVG_COLOR1BIT_ADDR 0x108
#define  TVG_COLOR2_ADDR    0x10c
#define  TVG_COLOR2BIT_ADDR 0x110
#define  TVG_STAT_ADDR      0x114
#define  TVG_STAT_CTRL_ADDR 0x144
#define  TVG_STAT_CLR_ADDR  0x164
#define  TVG_STAT_FLAG_ADDR 0x184

#define  DPI_IRQ_EN_ADDR   0x1a0
#define  DPI_IRQ_CLR_ADDR  0x1a4
#define  DPI_IRQ_STAT_ADDR 0x1a4
#define  DPI_CFG_ADDR      0x1ac

struct dcs_buffer {
    u32 len;
    union {
        u32 val32;
        char val8[4];
    };
};

extern int sf_mipi_init(struct sf_fb_data *sf_dev);

#endif

