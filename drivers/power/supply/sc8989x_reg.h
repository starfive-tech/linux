// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 Southchip Semiconductor Technology(Shanghai) Co., Ltd.
 */
#ifndef __SC89890H_HEADER__
#define __SC89890H_HEADER__

/* Register 00h */
#define SC89890H_REG_00                 0x00
#define SC89890H_ENHIZ_MASK             0x80
#define SC89890H_ENHIZ_SHIFT            7
#define SC89890H_HIZ_ENABLE             1
#define SC89890H_HIZ_DISABLE            0
#define SC89890H_ENILIM_MASK            0x40
#define SC89890H_ENILIM_SHIFT           6
#define SC89890H_ENILIM_ENABLE          1
#define SC89890H_ENILIM_DISABLE         0

#define SC89890H_IINLIM_MASK            0x3F
#define SC89890H_IINLIM_SHIFT           0
#define SC89890H_IINLIM_BASE            100
#define SC89890H_IINLIM_LSB             50

/* Register 01h */
#define SC89890H_REG_01                 0x01
#define SC89890H_DP_DRIVE_MASK          0xE0
#define SC89890H_DP_DRIVE_SHIFT         5
#define SC89890H_DM_DRIVE_MASK          0x1C
#define SC89890H_DM_DRIVE_SHIFT         2

#define SC89890H_HVDCP_5V               0x11
#define SC89890H_HVDCP_9V               0x32
#define SC89890H_HVDCP_12V              0x12

#define SC89890H_VINDPMOS_MASK          0x03
#define SC89890H_VINDPMOS_SHIFT         0
#define SC89890H_VINDPMOS_400MV         0
#define SC89890H_VINDPMOS_600MV         1

/* Register 0x02 */
#define SC89890H_REG_02                 0x02
#define SC89890H_CONV_START_MASK        0x80
#define SC89890H_CONV_START_SHIFT       7
#define SC89890H_CONV_START             0
#define SC89890H_CONV_RATE_MASK         0x40
#define SC89890H_CONV_RATE_SHIFT        6
#define SC89890H_ADC_CONTINUE_ENABLE    1
#define SC89890H_ADC_CONTINUE_DISABLE   0

#define SC89890H_BOOST_FREQ_MASK        0x20
#define SC89890H_BOOST_FREQ_SHIFT       5
#define SC89890H_BOOST_FREQ_1500K       0
#define SC89890H_BOOST_FREQ_500K        0

#define SC89890H_ICOEN_MASK             0x10
#define SC89890H_ICOEN_SHIFT            4
#define SC89890H_ICO_ENABLE             1
#define SC89890H_ICO_DISABLE            0

#define SC89890H_HVDCPEN_MASK           0x08
#define SC89890H_HVDCPEN_SHIFT          3
#define SC89890H_HVDCP_ENABLE           1
#define SC89890H_HVDCP_DISABLE          0

#define SC89890H_FORCE_DPDM_MASK        0x02
#define SC89890H_FORCE_DPDM_SHIFT       1
#define SC89890H_FORCE_DPDM             1

#define SC89890H_AUTO_DPDM_EN_MASK      0x01
#define SC89890H_AUTO_DPDM_EN_SHIFT     0
#define SC89890H_AUTO_DPDM_ENABLE       1
#define SC89890H_AUTO_DPDM_DISABLE      0

/* Register 0x03 */
#define SC89890H_REG_03                 0x03
#define SC89890H_FORCE_DSEL_MASK        0x80
#define SC89890H_FORCE_DSEL_SHIFT       7

#define SC89890H_WDT_RESET_MASK         0x40
#define SC89890H_WDT_RESET_SHIFT        6
#define SC89890H_WDT_RESET              1

#define SC89890H_OTG_CONFIG_MASK        0x20
#define SC89890H_OTG_CONFIG_SHIFT       5
#define SC89890H_OTG_ENABLE             1
#define SC89890H_OTG_DISABLE            0

#define SC89890H_CHG_CONFIG_MASK        0x10
#define SC89890H_CHG_CONFIG_SHIFT       4
#define SC89890H_CHG_ENABLE             1
#define SC89890H_CHG_DISABLE            0

#define SC89890H_SYS_MINV_MASK          0x0E
#define SC89890H_SYS_MINV_SHIFT         1
#define SC89890H_SYS_MINV_BASE          3000
#define SC89890H_SYS_MINV_LSB           100

#define SC89890H_VBAT_OTG_LOW_MASK      0x01
#define SC89890H_VBAT_OTG_LOW_SHIFT     0
#define SC89890H_VBAT_OTG_LOW_2V9       0
#define SC89890H_VBAT_OTG_LOW_2V5       1

/* Register 0x04*/
#define SC89890H_REG_04                 0x04
#define SC89890H_EN_PUMPX_MASK          0x80
#define SC89890H_EN_PUMPX_SHIFT         7
#define SC89890H_PUMPX_ENABLE           1
#define SC89890H_PUMPX_DISABLE          0

#define SC89890H_ICHG_MASK              0x7F
#define SC89890H_ICHG_SHIFT             0
#define SC89890H_ICHG_BASE              0
#define SC89890H_ICHG_LSB               60

/* Register 0x05*/
#define SC89890H_REG_05                 0x05
#define SC89890H_IPRECHG_MASK           0xF0
#define SC89890H_IPRECHG_SHIFT          4
#define SC89890H_IPRECHG_BASE           60
#define SC89890H_IPRECHG_LSB            60

#define SC89890H_ITERM_MASK             0x0F
#define SC89890H_ITERM_SHIFT            0
#define SC89890H_ITERM_BASE             30
#define SC89890H_ITERM_LSB              60

/* Register 0x06*/
#define SC89890H_REG_06                 0x06
#define SC89890H_VREG_MASK              0xFC
#define SC89890H_VREG_SHIFT             2
#define SC89890H_VREG_BASE              3840
#define SC89890H_VREG_LSB               16

#define SC89890H_BATLOWV_MASK           0x02
#define SC89890H_BATLOWV_SHIFT          1
#define SC89890H_BATLOWV_2800MV         0
#define SC89890H_BATLOWV_3000MV         1

#define SC89890H_VRECHG_MASK            0x01
#define SC89890H_VRECHG_SHIFT           0
#define SC89890H_VRECHG_100MV           0
#define SC89890H_VRECHG_200MV           1

/* Register 0x07*/
#define SC89890H_REG_07                 0x07
#define SC89890H_EN_TERM_MASK           0x80
#define SC89890H_EN_TERM_SHIFT          7
#define SC89890H_TERM_ENABLE            1
#define SC89890H_TERM_DISABLE           0

#define SC89890H_STAT_DIS_MASK          0x40
#define SC89890H_STAT_DIS_SHIFT         6
#define SC89890H_STAT_ENABLE            0
#define SC89890H_STAT_DISABLE           1

#define SC89890H_WDT_MASK               0x30
#define SC89890H_WDT_SHIFT              4
#define SC89890H_WDT_DISABLE            0
#define SC89890H_WDT_40S                1
#define SC89890H_WDT_80S                2
#define SC89890H_WDT_160S               3
#define SC89890H_WDT_BASE               0
#define SC89890H_WDT_LSB                40

#define SC89890H_EN_TIMER_MASK          0x08
#define SC89890H_EN_TIMER_SHIFT         3
#define SC89890H_CHG_TIMER_ENABLE       1
#define SC89890H_CHG_TIMER_DISABLE      0

#define SC89890H_CHG_TIMER_MASK         0x06
#define SC89890H_CHG_TIMER_SHIFT        1
#define SC89890H_CHG_TIMER_5HOURS       0
#define SC89890H_CHG_TIMER_8HOURS       1
#define SC89890H_CHG_TIMER_12HOURS      2
#define SC89890H_CHG_TIMER_20HOURS      3

#define SC89890H_JEITA_ISET_MASK        0x01
#define SC89890H_JEITA_ISET_SHIFT       0
#define SC89890H_JEITA_ISET_50PCT       0
#define SC89890H_JEITA_ISET_20PCT       1

/* Register 0x08*/
#define SC89890H_REG_08                 0x08
#define SC89890H_BAT_COMP_MASK          0xE0
#define SC89890H_BAT_COMP_SHIFT         5
#define SC89890H_BAT_COMP_BASE          0
#define SC89890H_BAT_COMP_LSB           20

#define SC89890H_VCLAMP_MASK            0x1C
#define SC89890H_VCLAMP_SHIFT           2
#define SC89890H_VCLAMP_BASE            0
#define SC89890H_VCLAMP_LSB             32

#define SC89890H_TREG_MASK              0x03
#define SC89890H_TREG_SHIFT             0
#define SC89890H_TREG_60C               0
#define SC89890H_TREG_80C               1
#define SC89890H_TREG_100C              2
#define SC89890H_TREG_120C              3

/* Register 0x09*/
#define SC89890H_REG_09                 0x09
#define SC89890H_FORCE_ICO_MASK         0x80
#define SC89890H_FORCE_ICO_SHIFT        7
#define SC89890H_FORCE_ICO              1

#define SC89890H_TMR2X_EN_MASK          0x40
#define SC89890H_TMR2X_EN_SHIFT         6

#define SC89890H_BATFET_DIS_MASK        0x20
#define SC89890H_BATFET_DIS_SHIFT       5
#define SC89890H_BATFET_OFF             1

#define SC89890H_JEITA_VSET_MASK        0x10
#define SC89890H_JEITA_VSET_SHIFT       4
#define SC89890H_JEITA_VSET_N150MV      0
#define SC89890H_JEITA_VSET_VREG        1

#define SC89890H_BATFET_DLY_MASK        0x08
#define SC89890H_BATFET_DLY_SHIFT       3

#define SC89890H_BATFET_RST_EN_MASK     0x04
#define SC89890H_BATFET_RST_EN_SHIFT    2

#define SC89890H_PUMPX_UP_MASK          0x02
#define SC89890H_PUMPX_UP_SHIFT         1
#define SC89890H_PUMPX_UP               1

#define SC89890H_PUMPX_DOWN_MASK        0x01
#define SC89890H_PUMPX_DOWN_SHIFT       0
#define SC89890H_PUMPX_DOWN             1

/* Register 0x0A*/
#define SC89890H_REG_0A                 0x0A
#define SC89890H_BOOSTV_MASK            0xF0
#define SC89890H_BOOSTV_SHIFT           4
#define SC89890H_BOOSTV_BASE            3900
#define SC89890H_BOOSTV_LSB             100

#define SC89890H_PFM_OTG_DIS_MASK       0x08
#define SC89890H_PFM_OTG_DIS_SHIFT      3

#define SC89890H_BOOST_LIM_MASK         0x07
#define SC89890H_BOOST_LIM_SHIFT        0
#define SC89890H_BOOST_LIM_500MA        0x00
#define SC89890H_BOOST_LIM_750MA        0x01
#define SC89890H_BOOST_LIM_1200MA       0x02
#define SC89890H_BOOST_LIM_1400MA       0x03
#define SC89890H_BOOST_LIM_1650MA       0x04
#define SC89890H_BOOST_LIM_1875MA       0x05
#define SC89890H_BOOST_LIM_2150MA       0x06
#define SC89890H_BOOST_LIM_2450MA       0x07

/* Register 0x0B*/
#define SC89890H_REG_0B                 0x0B
#define SC89890H_VBUS_STAT_MASK         0xE0
#define SC89890H_VBUS_STAT_SHIFT        5

#define SC89890H_CHRG_STAT_MASK         0x18
#define SC89890H_CHRG_STAT_SHIFT        3
#define SC89890H_CHRG_STAT_IDLE         0
#define SC89890H_CHRG_STAT_PRECHG       1
#define SC89890H_CHRG_STAT_FASTCHG      2
#define SC89890H_CHRG_STAT_CHGDONE      3

#define SC89890H_PG_STAT_MASK           0x04
#define SC89890H_PG_STAT_SHIFT          2

#define SC89890H_SDP_STAT_MASK          0x02
#define SC89890H_SDP_STAT_SHIFT         1

#define SC89890H_VSYS_STAT_MASK         0x01
#define SC89890H_VSYS_STAT_SHIFT        0

/* Register 0x0C*/
#define SC89890H_REG_0C                 0x0c
#define SC89890H_FAULT_WDT_MASK         0x80
#define SC89890H_FAULT_WDT_SHIFT        7

#define SC89890H_FAULT_BOOST_MASK       0x40
#define SC89890H_FAULT_BOOST_SHIFT      6

#define SC89890H_FAULT_CHRG_MASK        0x30
#define SC89890H_FAULT_CHRG_SHIFT       4
#define SC89890H_FAULT_CHRG_NORMAL      0
#define SC89890H_FAULT_CHRG_INPUT       1
#define SC89890H_FAULT_CHRG_THERMAL     2
#define SC89890H_FAULT_CHRG_TIMER       3

#define SC89890H_FAULT_BAT_MASK         0x08
#define SC89890H_FAULT_BAT_SHIFT        3

#define SC89890H_FAULT_NTC_MASK         0x07
#define SC89890H_FAULT_NTC_SHIFT        0
#define SC89890H_FAULT_NTC_NORMAL       0
#define SC89890H_FAULT_NTC_WARM         2
#define SC89890H_FAULT_NTC_COOL         3
#define SC89890H_FAULT_NTC_COLD         5
#define SC89890H_FAULT_NTC_HOT          6

/* Register 0x0D*/
#define SC89890H_REG_0D                 0x0D
#define SC89890H_FORCE_VINDPM_MASK      0x80
#define SC89890H_FORCE_VINDPM_SHIFT     7
#define SC89890H_FORCE_VINDPM_ENABLE    1
#define SC89890H_FORCE_VINDPM_DISABLE   0

#define SC89890H_VINDPM_MASK            0x7F
#define SC89890H_VINDPM_SHIFT           0
#define SC89890H_VINDPM_BASE            2600
#define SC89890H_VINDPM_LSB             100

/* Register 0x0E*/
#define SC89890H_REG_0E                 0x0E
#define SC89890H_THERM_STAT_MASK        0x80
#define SC89890H_THERM_STAT_SHIFT       7

#define SC89890H_BATV_MASK              0x7F
#define SC89890H_BATV_SHIFT             0
#define SC89890H_BATV_BASE              2304
#define SC89890H_BATV_LSB               20

/* Register 0x0F*/
#define SC89890H_REG_0F                 0x0F
#define SC89890H_SYSV_MASK              0x7F
#define SC89890H_SYSV_SHIFT             0
#define SC89890H_SYSV_BASE              2304
#define SC89890H_SYSV_LSB               20

/* Register 0x10*/
#define SC89890H_REG_10                 0x10
#define SC89890H_TSPCT_MASK             0x7F
#define SC89890H_TSPCT_SHIFT            0
#define SC89890H_TSPCT_BASE             21
#define SC89890H_TSPCT_LSB              (47 / 100)

/* Register 0x11*/
#define SC89890H_REG_11                 0x11
#define SC89890H_VBUS_GD_MASK           0x80
#define SC89890H_VBUS_GD_SHIFT          7

#define SC89890H_VBUSV_MASK             0x7F
#define SC89890H_VBUSV_SHIFT            0
#define SC89890H_VBUSV_BASE             2600
#define SC89890H_VBUSV_LSB              100

/* Register 0x12*/
#define SC89890H_REG_12                 0x12
#define SC89890H_ICHGR_MASK             0x7F
#define SC89890H_ICHGR_SHIFT            0
#define SC89890H_ICHGR_BASE             0
#define SC89890H_ICHGR_LSB              50

/* Register 0x13*/
#define SC89890H_REG_13                 0x13
#define SC89890H_VDPM_STAT_MASK         0x80
#define SC89890H_VDPM_STAT_SHIFT        7

#define SC89890H_IDPM_STAT_MASK         0x40
#define SC89890H_IDPM_STAT_SHIFT        6

#define SC89890H_IDPM_LIM_MASK          0x3F
#define SC89890H_IDPM_LIM_SHIFT         0
#define SC89890H_IDPM_LIM_BASE          100
#define SC89890H_IDPM_LIM_LSB           50

/* Register 0x14*/
#define SC89890H_REG_14                 0x14
#define SC89890H_RESET_MASK             0x80
#define SC89890H_RESET_SHIFT            7
#define SC89890H_RESET                  1

#define SC89890H_ICO_OPTIMIZED_MASK     0x40
#define SC89890H_ICO_OPTIMIZED_SHIFT    6
#define SC89890H_PN_MASK                0x38
#define SC89890H_PN_SHIFT               3

#endif
