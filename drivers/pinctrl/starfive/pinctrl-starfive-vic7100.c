/**
  ******************************************************************************
  * @file  pinctrl-starfive-vic7100.c
  * @author  StarFive Technology
  * @version  V1.0
  * @date  11/19/2021
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

#include <linux/err.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/gpio/driver.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>

#include "../core.h"
#include "../pinconf.h"
#include "../pinmux.h"
#include "pinctrl-starfive.h"

#define IO_PAD_SCFG_QSPI_IOCTRL			0x19c
#define IO_PADSHARE_SEL_REG_REG			0x1a0
#define IO_PAD_CTRL_SEL_FUNC_0_REG_ADDR    	0x0
#define IO_PAD_CTRL_SEL_FUNC_1_REG_ADDR    	0x0
#define IO_PAD_CTRL_SEL_FUNC_2_REG_ADDR    	0x110
#define IO_PAD_CTRL_SEL_FUNC_3_REG_ADDR    	0x10c
#define IO_PAD_CTRL_SEL_FUNC_4_REG_ADDR    	0x80
#define IO_PAD_CTRL_SEL_FUNC_5_REG_ADDR    	0x80
#define IO_PAD_CTRL_SEL_FUNC_6_REG_ADDR    	0x80
#define IO_PADSHARE_SEL_REG_MAX		   	6

/*gpio dout/don/din reg*/
#define GPO_DOUT_CFG_BASE_REG 			0x50
#define GPO_DOEN_CFG_BASE_REG 			0x54
#define GPI_DIN_CFG_BASE_REG			0x250

#define GPO_DOUT_CFG_REG_OFFSET 		8
#define GPO_DOEN_CFG_REG_OFFSET 		8
#define GPI_DIN_CFG_REG_OFFSET 			4

#define GPI_DIN_ADDR_END			74

#define GPIO_EN        				0x0
#define GPIO_IS_LOW    				0x10
#define GPIO_IS_HIGH   				0x14
#define GPIO_IBE_LOW   				0x18
#define GPIO_IBE_HIGH  				0x1c
#define GPIO_IEV_LOW   				0x20
#define GPIO_IEV_HIGH  				0x24
#define GPIO_IE_LOW    				0x28
#define GPIO_IE_HIGH  				0x2c
#define GPIO_IC_LOW    				0x30
#define GPIO_IC_HIGH   				0x34
//read only
#define GPIO_RIS_LOW   				0x38
#define GPIO_RIS_HIGH  				0x3c
#define GPIO_MIS_LOW   				0x40
#define GPIO_MIS_HIGH  				0x44
#define GPIO_DIN_LOW   				0x48
#define GPIO_DIN_HIGH  				0x4c


enum starfive_vic700_pads {
	PAD_GPIO_0 = 0,
	PAD_GPIO_1 = 1,
	PAD_GPIO_2 = 2,
	PAD_GPIO_3 = 3,
	PAD_GPIO_4 = 4,
	PAD_GPIO_5 = 5,
	PAD_GPIO_6 = 6,
	PAD_GPIO_7 = 7,
	PAD_GPIO_8 = 8,
	PAD_GPIO_9 = 9,
	PAD_GPIO_10 = 10,
	PAD_GPIO_11 = 11,
	PAD_GPIO_12 = 12,
	PAD_GPIO_13 = 13,
	PAD_GPIO_14 = 14,
	PAD_GPIO_15 = 15,
	PAD_GPIO_16 = 16,
	PAD_GPIO_17 = 17,
	PAD_GPIO_18 = 18,
	PAD_GPIO_19 = 19,
	PAD_GPIO_20 = 20,
	PAD_GPIO_21 = 21,
	PAD_GPIO_22 = 22,
	PAD_GPIO_23 = 23,
	PAD_GPIO_24 = 24,
	PAD_GPIO_25 = 25,
	PAD_GPIO_26 = 26,
	PAD_GPIO_27 = 27,
	PAD_GPIO_28 = 28,
	PAD_GPIO_29 = 29,
	PAD_GPIO_30 = 30,
	PAD_GPIO_31 = 31,
	PAD_GPIO_32 = 32,
	PAD_GPIO_33 = 33,
	PAD_GPIO_34 = 34,
	PAD_GPIO_35 = 35,
	PAD_GPIO_36 = 36,
	PAD_GPIO_37 = 37,
	PAD_GPIO_38 = 38,
	PAD_GPIO_39 = 39,
	PAD_GPIO_40 = 40,
	PAD_GPIO_41 = 41,
	PAD_GPIO_42 = 42,
	PAD_GPIO_43 = 43,
	PAD_GPIO_44 = 44,
	PAD_GPIO_45 = 45,
	PAD_GPIO_46 = 46,
	PAD_GPIO_47 = 47,
	PAD_GPIO_48 = 48,
	PAD_GPIO_49 = 49,
	PAD_GPIO_50 = 50,
	PAD_GPIO_51 = 51,
	PAD_GPIO_52 = 52,
	PAD_GPIO_53 = 53,
	PAD_GPIO_54 = 54,
	PAD_GPIO_55 = 55,
	PAD_GPIO_56 = 56,
	PAD_GPIO_57 = 57,
	PAD_GPIO_58 = 58,
	PAD_GPIO_59 = 59,
	PAD_GPIO_60 = 60,
	PAD_GPIO_61 = 61,
	PAD_GPIO_62 = 62,
	PAD_GPIO_63 = 63,
	PAD_FUNC_SHARE_0 = 64,
	PAD_FUNC_SHARE_1 = 65,
	PAD_FUNC_SHARE_2 = 66,
	PAD_FUNC_SHARE_3 = 67,
	PAD_FUNC_SHARE_4 = 68,
	PAD_FUNC_SHARE_5 = 69,
	PAD_FUNC_SHARE_6 = 70,
	PAD_FUNC_SHARE_7 = 71,
	PAD_FUNC_SHARE_8 = 72,
	PAD_FUNC_SHARE_9 = 73,
	PAD_FUNC_SHARE_10 = 74,
	PAD_FUNC_SHARE_11 = 75,
	PAD_FUNC_SHARE_12 = 76,
	PAD_FUNC_SHARE_13 = 77,
	PAD_FUNC_SHARE_14 = 78,
	PAD_FUNC_SHARE_15 = 79,
	PAD_FUNC_SHARE_16 = 80,
	PAD_FUNC_SHARE_17 = 81,
	PAD_FUNC_SHARE_18 = 82,
	PAD_FUNC_SHARE_19 = 83,
	PAD_FUNC_SHARE_20 = 84,
	PAD_FUNC_SHARE_21 = 85,
	PAD_FUNC_SHARE_22 = 86,
	PAD_FUNC_SHARE_23 = 87,
	PAD_FUNC_SHARE_24 = 88,
	PAD_FUNC_SHARE_25 = 89,
	PAD_FUNC_SHARE_26 = 90,
	PAD_FUNC_SHARE_27 = 91,
	PAD_FUNC_SHARE_28 = 92,
	PAD_FUNC_SHARE_29 = 93,
	PAD_FUNC_SHARE_30 = 94,
	PAD_FUNC_SHARE_31 = 95,
	PAD_FUNC_SHARE_32 = 96,
	PAD_FUNC_SHARE_33 = 97,
	PAD_FUNC_SHARE_34 = 98,
	PAD_FUNC_SHARE_35 = 99,
	PAD_FUNC_SHARE_36 = 100,
	PAD_FUNC_SHARE_37 = 101,
	PAD_FUNC_SHARE_38 = 102,
	PAD_FUNC_SHARE_39 = 103,
	PAD_FUNC_SHARE_40 = 104,
	PAD_FUNC_SHARE_41 = 105,
	PAD_FUNC_SHARE_42 = 106,
	PAD_FUNC_SHARE_43 = 107,
	PAD_FUNC_SHARE_44 = 108,
	PAD_FUNC_SHARE_45 = 109,
	PAD_FUNC_SHARE_46 = 110,
	PAD_FUNC_SHARE_47 = 111,
	PAD_FUNC_SHARE_48 = 112,
	PAD_FUNC_SHARE_49 = 113,
	PAD_FUNC_SHARE_50 = 114,
	PAD_FUNC_SHARE_51 = 115,
	PAD_FUNC_SHARE_52 = 116,
	PAD_FUNC_SHARE_53 = 117,
	PAD_FUNC_SHARE_54 = 118,
	PAD_FUNC_SHARE_55 = 119,
	PAD_FUNC_SHARE_56 = 120,
	PAD_FUNC_SHARE_57 = 121,
	PAD_FUNC_SHARE_58 = 122,
	PAD_FUNC_SHARE_59 = 123,
	PAD_FUNC_SHARE_60 = 124,
	PAD_FUNC_SHARE_61 = 125,
	PAD_FUNC_SHARE_62 = 126,
	PAD_FUNC_SHARE_63 = 127,
	PAD_FUNC_SHARE_64 = 128,
	PAD_FUNC_SHARE_65 = 129,
	PAD_FUNC_SHARE_66 = 130,
	PAD_FUNC_SHARE_67 = 131,
	PAD_FUNC_SHARE_68 = 132,
	PAD_FUNC_SHARE_69 = 133,
	PAD_FUNC_SHARE_70 = 134,
	PAD_FUNC_SHARE_71 = 135,
	PAD_FUNC_SHARE_72 = 136,
	PAD_FUNC_SHARE_73 = 137,
	PAD_FUNC_SHARE_74 = 138,
	PAD_FUNC_SHARE_75 = 139,
	PAD_FUNC_SHARE_76 = 140,
	PAD_FUNC_SHARE_77 = 141,
	PAD_FUNC_SHARE_78 = 142,
	PAD_FUNC_SHARE_79 = 143,
	PAD_FUNC_SHARE_80 = 144,
	PAD_FUNC_SHARE_81 = 145,
	PAD_FUNC_SHARE_82 = 146,
	PAD_FUNC_SHARE_83 = 147,
	PAD_FUNC_SHARE_84 = 148,
	PAD_FUNC_SHARE_85 = 149,
	PAD_FUNC_SHARE_86 = 150,
	PAD_FUNC_SHARE_87 = 151,
	PAD_FUNC_SHARE_88 = 152,
	PAD_FUNC_SHARE_89 = 153,
	PAD_FUNC_SHARE_90 = 154,
	PAD_FUNC_SHARE_91 = 155,
	PAD_FUNC_SHARE_92 = 156,
	PAD_FUNC_SHARE_93 = 157,
	PAD_FUNC_SHARE_94 = 158,
	PAD_FUNC_SHARE_95 = 159,
	PAD_FUNC_SHARE_96 = 160,
	PAD_FUNC_SHARE_97 = 161,
	PAD_FUNC_SHARE_98 = 162,
	PAD_FUNC_SHARE_99 = 163,
	PAD_FUNC_SHARE_100 = 164,
	PAD_FUNC_SHARE_101 = 165,
	PAD_FUNC_SHARE_102 = 166,
	PAD_FUNC_SHARE_103 = 167,
	PAD_FUNC_SHARE_104 = 168,
	PAD_FUNC_SHARE_105 = 169,
	PAD_FUNC_SHARE_106 = 170,
	PAD_FUNC_SHARE_107 = 171,
	PAD_FUNC_SHARE_108 = 172,
	PAD_FUNC_SHARE_109 = 173,
	PAD_FUNC_SHARE_110 = 174,
	PAD_FUNC_SHARE_111 = 175,
	PAD_FUNC_SHARE_112 = 176,
	PAD_FUNC_SHARE_113 = 177,
	PAD_FUNC_SHARE_114 = 178,
	PAD_FUNC_SHARE_115 = 179,
	PAD_FUNC_SHARE_116 = 180,
	PAD_FUNC_SHARE_117 = 181,
	PAD_FUNC_SHARE_118 = 182,
	PAD_FUNC_SHARE_119 = 183,
	PAD_FUNC_SHARE_120 = 184,
	PAD_FUNC_SHARE_121 = 185,
	PAD_FUNC_SHARE_122 = 186,
	PAD_FUNC_SHARE_123 = 187,
	PAD_FUNC_SHARE_124 = 188,
	PAD_FUNC_SHARE_125 = 189,
	PAD_FUNC_SHARE_126 = 190,
	PAD_FUNC_SHARE_127 = 191,
	PAD_FUNC_SHARE_128 = 192,
	PAD_FUNC_SHARE_129 = 193,
	PAD_FUNC_SHARE_130 = 194,
	PAD_FUNC_SHARE_131 = 195,
	PAD_FUNC_SHARE_132 = 196,
	PAD_FUNC_SHARE_133 = 197,
	PAD_FUNC_SHARE_134 = 198,
	PAD_FUNC_SHARE_135 = 199,
	PAD_FUNC_SHARE_136 = 200,
	PAD_FUNC_SHARE_137 = 201,
	PAD_FUNC_SHARE_138 = 202,
	PAD_FUNC_SHARE_139 = 203,
	PAD_FUNC_SHARE_140 = 204,
	PAD_FUNC_SHARE_141 = 205,
};


/* Pad names for the pinmux subsystem */
static const struct pinctrl_pin_desc starfive_vic7100_pinctrl_pads[] = {
	STARFIVE_PINCTRL_PIN(PAD_GPIO_0),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_1),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_2),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_3),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_4),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_5),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_6),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_7),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_8),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_9),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_10),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_11),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_12),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_13),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_14),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_15),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_16),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_17),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_18),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_19),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_20),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_21),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_22),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_23),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_24),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_25),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_26),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_27),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_28),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_29),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_30),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_31),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_32),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_33),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_34),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_35),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_36),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_37),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_38),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_39),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_40),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_41),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_42),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_43),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_44),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_45),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_46),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_47),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_48),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_49),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_50),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_51),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_52),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_53),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_54),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_55),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_56),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_57),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_58),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_59),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_60),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_61),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_62),
	STARFIVE_PINCTRL_PIN(PAD_GPIO_63),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_0),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_1),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_2),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_3),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_4),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_5),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_6),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_7),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_8),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_9),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_10),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_11),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_12),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_13),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_14),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_15),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_16),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_17),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_18),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_19),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_20),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_21),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_22),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_23),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_24),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_25),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_26),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_27),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_28),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_29),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_30),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_31),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_32),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_33),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_34),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_35),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_36),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_37),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_38),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_39),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_40),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_41),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_42),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_43),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_44),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_45),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_46),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_47),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_48),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_49),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_50),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_51),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_52),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_53),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_54),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_55),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_56),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_57),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_58),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_59),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_60),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_61),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_62),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_63),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_64),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_65),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_66),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_67),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_68),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_69),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_70),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_71),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_72),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_73),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_74),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_75),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_76),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_77),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_78),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_79),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_80),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_81),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_82),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_83),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_84),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_85),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_86),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_87),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_88),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_89),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_90),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_91),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_92),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_93),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_94),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_95),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_96),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_97),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_98),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_99),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_100),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_101),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_102),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_103),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_104),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_105),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_106),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_107),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_108),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_109),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_110),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_111),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_112),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_113),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_114),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_115),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_116),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_117),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_118),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_119),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_120),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_121),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_122),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_123),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_124),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_125),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_126),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_127),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_128),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_129),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_130),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_131),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_132),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_133),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_134),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_135),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_136),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_137),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_138),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_139),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_140),
	STARFIVE_PINCTRL_PIN(PAD_FUNC_SHARE_141),
};
	
static unsigned int starfive_vic7100_gpio_to_pin(const struct starfive_pinctrl *chip,
					 unsigned int gpio)
{
	return gpio + chip->gpios.pin_base;
}

static void starfive_vic7100_padctl_rmw(struct starfive_pinctrl *chip,
				unsigned int pin,
				u16 _mask, u16 _value)
{
	void __iomem *reg = chip->padctl_base + 4 * (pin / 2);
	u32 mask = _mask;
	u32 value = _value;
	unsigned long flags;

	if (pin & 1U) {
		value <<= 16;
		mask <<= 16;
	}

	raw_spin_lock_irqsave(&chip->lock, flags);
	value |= readl_relaxed(reg) & ~mask;
	writel_relaxed(value, reg);
	raw_spin_unlock_irqrestore(&chip->lock, flags);
}


static int starfive_vic7100_direction_input(struct gpio_chip *gc, unsigned gpio)
{
	struct starfive_pinctrl *chip = gpiochip_get_data(gc);
	unsigned long flags;

	if (gpio >= gc->ngpio)
		return -EINVAL;
	
	/* enable input and schmitt trigger */
	starfive_vic7100_padctl_rmw(chip, starfive_vic7100_gpio_to_pin(chip, gpio),
			PAD_INPUT_ENABLE | PAD_INPUT_SCHMITT_ENABLE,
			PAD_INPUT_ENABLE | PAD_INPUT_SCHMITT_ENABLE);

	raw_spin_lock_irqsave(&chip->lock, flags);
	writel_relaxed(0x1, chip->gpio_base + GPO_DOEN_CFG_BASE_REG + gpio * 8);
	raw_spin_unlock_irqrestore(&chip->lock, flags);

	return 0;
}

static int starfive_vic7100_direction_output(struct gpio_chip *gc, unsigned gpio, int value)
{
	struct starfive_pinctrl *chip = gpiochip_get_data(gc);
	unsigned long flags;

	if (gpio >= gc->ngpio)
		return -EINVAL;
	
	raw_spin_lock_irqsave(&chip->lock, flags);
	writel_relaxed(0x0, chip->gpio_base + GPO_DOEN_CFG_BASE_REG + gpio * 8);
	writel_relaxed(value, chip->gpio_base + GPO_DOUT_CFG_BASE_REG + gpio * 8);
	raw_spin_unlock_irqrestore(&chip->lock, flags);

	/* disable input, schmitt trigger and bias */
	starfive_vic7100_padctl_rmw(chip, starfive_vic7100_gpio_to_pin(chip, gpio),
			PAD_BIAS_MASK | PAD_INPUT_ENABLE | PAD_INPUT_SCHMITT_ENABLE,
			PAD_BIAS_DISABLE);

	return 0;
}

static int starfive_vic7100_get_direction(struct gpio_chip *gc, unsigned gpio)
{
	struct starfive_pinctrl *chip = gpiochip_get_data(gc);

	if (gpio >= gc->ngpio)
		return -EINVAL;

	return readl_relaxed(chip->gpio_base + GPO_DOEN_CFG_BASE_REG + gpio * 8) & 0x1;
}

static int starfive_vic7100_get_value(struct gpio_chip *gc, unsigned gpio)
{
	struct starfive_pinctrl *chip = gpiochip_get_data(gc);
	int value;

	if (gpio >= gc->ngpio)
		return -EINVAL;

	if(gpio < 32){
		value = readl_relaxed(chip->gpio_base + GPIO_DIN_LOW);
		return (value >> gpio) & 0x1;
	} else {
		value = readl_relaxed(chip->gpio_base + GPIO_DIN_HIGH);
		return (value >> (gpio - 32)) & 0x1;
	}
}

static void starfive_vic7100_set_value(struct gpio_chip *gc, unsigned gpio, int value)
{
	struct starfive_pinctrl *chip = gpiochip_get_data(gc);
	unsigned long flags;

	if (gpio >= gc->ngpio)
		return;

	raw_spin_lock_irqsave(&chip->lock, flags);
	writel_relaxed(value, chip->gpio_base + GPO_DOUT_CFG_BASE_REG + gpio * 8);
	raw_spin_unlock_irqrestore(&chip->lock, flags);
}

static void starfive_vic7100_set_ie(struct starfive_pinctrl *chip, int gpio)
{
	unsigned long flags;
	int old_value, new_value;
	int reg_offset, index;

	if(gpio < 32) {
		reg_offset = 0;
		index = gpio;
	} else {
		reg_offset = 4;
		index = gpio - 32;
	}
	raw_spin_lock_irqsave(&chip->lock, flags);
	old_value = readl_relaxed(chip->gpio_base + GPIO_IE_LOW + reg_offset);
	new_value = old_value | ( 1 << index);
	writel_relaxed(new_value, chip->gpio_base + GPIO_IE_LOW + reg_offset);
	raw_spin_unlock_irqrestore(&chip->lock, flags);
}

static int starfive_vic7100_irq_set_type(struct irq_data *d, unsigned trigger)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct starfive_pinctrl *chip = gpiochip_get_data(gc);
	int offset = irqd_to_hwirq(d);
	unsigned int reg_is, reg_ibe, reg_iev;
	int reg_offset, index;

	if (offset < 0 || offset >= gc->ngpio)
		return -EINVAL;

	if(offset < 32) {
		reg_offset = 0;
		index = offset;
	} else {
		reg_offset = 4;
		index = offset - 32;
	}
	switch(trigger) {
	case IRQ_TYPE_LEVEL_HIGH:
		reg_is = readl_relaxed(chip->gpio_base + GPIO_IS_LOW + reg_offset);
		reg_ibe = readl_relaxed(chip->gpio_base + GPIO_IBE_LOW + reg_offset);
		reg_iev = readl_relaxed(chip->gpio_base + GPIO_IEV_LOW + reg_offset);
		reg_is  &= (~(0x1<< index));
		reg_ibe &= (~(0x1<< index));
		reg_iev |= (0x1<< index);
		writel_relaxed(reg_is, chip->gpio_base + GPIO_IS_LOW + reg_offset);
		writel_relaxed(reg_is, chip->gpio_base + GPIO_IS_LOW + reg_offset);
		writel_relaxed(reg_is, chip->gpio_base + GPIO_IS_LOW + reg_offset);
		break;
	case IRQ_TYPE_LEVEL_LOW:
		reg_is = readl_relaxed(chip->gpio_base + GPIO_IS_LOW + reg_offset);
		reg_ibe = readl_relaxed(chip->gpio_base + GPIO_IBE_LOW + reg_offset);
		reg_iev = readl_relaxed(chip->gpio_base + GPIO_IEV_LOW + reg_offset);
		reg_is  &= (~(0x1<< index));
		reg_ibe &= (~(0x1<< index));
		reg_iev &= (0x1<< index);
		writel_relaxed(reg_is, chip->gpio_base + GPIO_IS_LOW + reg_offset);
		writel_relaxed(reg_is, chip->gpio_base + GPIO_IS_LOW + reg_offset);
		writel_relaxed(reg_is, chip->gpio_base + GPIO_IS_LOW + reg_offset);
		break;
	case IRQ_TYPE_EDGE_BOTH:
		reg_is = readl_relaxed(chip->gpio_base + GPIO_IS_LOW + reg_offset);
		reg_ibe = readl_relaxed(chip->gpio_base + GPIO_IBE_LOW + reg_offset);
		//reg_iev = readl_relaxed(chip->gpio_base + GPIO_IEV_LOW + reg_offset);
		reg_is  |= (~(0x1<< index));
		reg_ibe |= (~(0x1<< index));
		//reg_iev |= (0x1<< index);
		writel_relaxed(reg_is, chip->gpio_base + GPIO_IS_LOW + reg_offset);
		writel_relaxed(reg_is, chip->gpio_base + GPIO_IS_LOW + reg_offset);
		//writel_relaxed(reg_is, chip->gpio_base + GPIO_IS_LOW + reg_offset);
		break;
	case IRQ_TYPE_EDGE_RISING:
		reg_is = readl_relaxed(chip->gpio_base + GPIO_IS_LOW + reg_offset);
		reg_ibe = readl_relaxed(chip->gpio_base + GPIO_IBE_LOW + reg_offset);
		reg_iev = readl_relaxed(chip->gpio_base + GPIO_IEV_LOW + reg_offset);
		reg_is  |= (~(0x1<< index));
		reg_ibe &= (~(0x1<< index));
		reg_iev |= (0x1<< index);
		writel_relaxed(reg_is, chip->gpio_base + GPIO_IS_LOW + reg_offset);
		writel_relaxed(reg_is, chip->gpio_base + GPIO_IS_LOW + reg_offset);
		writel_relaxed(reg_is, chip->gpio_base + GPIO_IS_LOW + reg_offset);
		break;
	case IRQ_TYPE_EDGE_FALLING:
		reg_is = readl_relaxed(chip->gpio_base + GPIO_IS_LOW + reg_offset);
		reg_ibe = readl_relaxed(chip->gpio_base + GPIO_IBE_LOW + reg_offset);
		reg_iev = readl_relaxed(chip->gpio_base + GPIO_IEV_LOW + reg_offset);
		reg_is  |= (~(0x1<< index));
		reg_ibe &= (~(0x1<< index));
		reg_iev &= (0x1<< index);
		writel_relaxed(reg_is, chip->gpio_base + GPIO_IS_LOW + reg_offset);
		writel_relaxed(reg_is, chip->gpio_base + GPIO_IS_LOW + reg_offset);
		writel_relaxed(reg_is, chip->gpio_base + GPIO_IS_LOW + reg_offset);
		break;
	}

	chip->trigger[offset] = trigger;
	starfive_vic7100_set_ie(chip, offset);
	return 0;
}


/* chained_irq_{enter,exit} already mask the parent */
static void starfive_vic7100_irq_mask(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct starfive_pinctrl *chip = gpiochip_get_data(gc);
	unsigned int value;
	int offset = irqd_to_hwirq(d);
	int reg_offset, index;

	if (offset < 0 || offset >= gc->ngpio)
		return;

	if(offset < 32) {
		reg_offset = 0;
		index = offset;
	} else {
		reg_offset = 4;
		index = offset - 32;
	}

	value = readl_relaxed(chip->gpio_base + GPIO_IE_LOW + reg_offset);
	value &= ~(0x1 << index);
	writel_relaxed(value,chip->gpio_base + GPIO_IE_LOW + reg_offset);
}

static void starfive_vic7100_irq_unmask(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct starfive_pinctrl *chip = gpiochip_get_data(gc);
	unsigned int value;
	int offset = irqd_to_hwirq(d);
	int reg_offset, index;

	if (offset < 0 || offset >= gc->ngpio)
		return;

	if(offset < 32) {
		reg_offset = 0;
		index = offset;
	} else {
		reg_offset = 4;
		index = offset - 32;
	}

	value = readl_relaxed(chip->gpio_base + GPIO_IE_LOW + reg_offset);
	value |= (0x1 << index);
	writel_relaxed(value,chip->gpio_base + GPIO_IE_LOW + reg_offset);
}

static void starfive_vic7100_irq_enable(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct starfive_pinctrl *chip = gpiochip_get_data(gc);
	int offset = irqd_to_hwirq(d);

	starfive_vic7100_irq_unmask(d);
	assign_bit(offset, &chip->enabled, 1);
}

static void starfive_vic7100_irq_disable(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct starfive_pinctrl *chip = gpiochip_get_data(gc);
	int offset = irqd_to_hwirq(d) % MAX_GPIO; // must not fail

	assign_bit(offset, &chip->enabled, 0);
	starfive_vic7100_set_ie(chip, offset);
}

static struct irq_chip starfive_irqchip = {
	.name		= "starfive-gpio",
	.irq_set_type	= starfive_vic7100_irq_set_type,
	.irq_mask	= starfive_vic7100_irq_mask,
	.irq_unmask	= starfive_vic7100_irq_unmask,
	.irq_enable	= starfive_vic7100_irq_enable,
	.irq_disable	= starfive_vic7100_irq_disable,
};

static irqreturn_t starfive_vic7100_irq_handler(int irq, void *gc)
{
	int offset;
	// = self_ptr - &chip->self_ptr[0];
	int reg_offset, index;
	unsigned int value;
	unsigned long flags;
	struct starfive_pinctrl *chip = gc;

	for (offset = 0; offset < 64; offset++) {
		if(offset < 32) {
			reg_offset = 0;
			index = offset;
		} else {
			reg_offset = 4;
			index = offset - 32;
		}

		raw_spin_lock_irqsave(&chip->lock, flags);
		value = readl_relaxed(chip->gpio_base + GPIO_MIS_LOW + reg_offset);
		if(value & BIT(index))
			writel_relaxed(BIT(index), chip->gpio_base + GPIO_IC_LOW +
						   reg_offset);

		//generic_handle_irq(irq_find_mapping(chip->gc.irq.domain,
		//				      offset));
		raw_spin_unlock_irqrestore(&chip->lock, flags);
	}

	return IRQ_HANDLED;
}


static int starfive_vic7100_gpio_register(struct platform_device *pdev,
					struct starfive_pinctrl *ipctl)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	int irq, ret, ngpio;

	ngpio = MAX_GPIO;
	
	ipctl->gc.direction_input = starfive_vic7100_direction_input;
	ipctl->gc.direction_output = starfive_vic7100_direction_output;
	ipctl->gc.get_direction = starfive_vic7100_get_direction;
	ipctl->gc.get = starfive_vic7100_get_value;
	ipctl->gc.set = starfive_vic7100_set_value;
	ipctl->gc.base = 0;
	ipctl->gc.ngpio = ngpio;
	ipctl->gc.label = dev_name(dev);
	ipctl->gc.parent = dev;
	ipctl->gc.owner = THIS_MODULE;

	ret = gpiochip_add_data(&ipctl->gc, ipctl);
	if (ret){
		dev_err(dev, "gpiochip_add_data ret=%d!\n", ret);
		return ret;
	}

	/* Disable all GPIO interrupts before enabling parent interrupts */
	iowrite32(0, ipctl->gpio_base + GPIO_IE_HIGH);
	iowrite32(0, ipctl->gpio_base + GPIO_IE_LOW);
	ipctl->enabled = 0;

	ret = gpiochip_irqchip_add(&ipctl->gc, &starfive_irqchip, 0,
				   handle_simple_irq, IRQ_TYPE_NONE);
	if (ret) {
		dev_err(dev, "could not add irqchip\n");
		gpiochip_remove(&ipctl->gc);
		return ret;
	}
	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(dev, "Cannot get IRQ resource\n");
		return irq;
	}

	ret = devm_request_irq(dev, irq, starfive_vic7100_irq_handler, IRQF_SHARED,
			       dev_name(dev), ipctl);
	if (ret) {
		dev_err(dev, "IRQ handler registering failed (%d)\n", ret);
		return ret;
	}

	writel_relaxed(1, ipctl->gpio_base + GPIO_EN);

	return 0;
}


static int starfive_vic7100_pinconf_get(struct pinctrl_dev *pctldev, unsigned pin_id,
				unsigned long *config)
{
	struct starfive_pinctrl *ipctl = pinctrl_dev_get_drvdata(pctldev);
	const struct starfive_pinctrl_soc_info *info = ipctl->info;
	const struct starfive_pin_reg *pin_reg = &ipctl->pin_regs[pin_id];
	unsigned int offset_reg;
	u32 value;
		
	if (pin_reg->io_conf_reg == -1) {
		dev_err(ipctl->dev, "Pin(%s) does not support config function\n",
			info->pins[pin_id].name);
		return -EINVAL;
	}
	offset_reg = (pin_reg->io_conf_reg/2)*4;
	value = readl_relaxed(ipctl->padctl_base + offset_reg);

	if(pin_reg->io_conf_reg%2)
		*config = value >> 16; //high 16bit
	else
		*config = value & 0xFFFF; //low 16bit

	return 0;
}

		       
static int starfive_vic7100_pinconf_set(struct pinctrl_dev *pctldev,
				unsigned pin_id, unsigned long *configs,
				unsigned num_configs)
{
	struct starfive_pinctrl *ipctl = pinctrl_dev_get_drvdata(pctldev);
	const struct starfive_pinctrl_soc_info *info = ipctl->info;
	const struct starfive_pin_reg *pin_reg = &ipctl->pin_regs[pin_id];
	int i;
	unsigned int offset_reg;
	u32 value;
	unsigned long flags;

	if (pin_reg->io_conf_reg == -1) {
		dev_err(ipctl->dev, "Pin(%s) does not support config function\n",
			info->pins[pin_id].name);
		return -EINVAL;
	}
	
	raw_spin_lock_irqsave(&ipctl->lock, flags);
	for (i = 0; i < num_configs; i++) {
	
		offset_reg = (pin_reg->io_conf_reg/2)*4;
		value = readl_relaxed(ipctl->padctl_base + offset_reg);
	
		if(pin_reg->io_conf_reg%2)
			value = value|((configs[i] & 0xFFFF) << 16);//high 16bit
		else
			value = value|(configs[i] & 0xFFFF);//low 16bit
	
		writel_relaxed(value, ipctl->padctl_base + offset_reg);
	} 
	raw_spin_unlock_irqrestore(&ipctl->lock, flags);

	return 0;
}

static int starfive_vic7100_iopad_sel_func(struct starfive_pinctrl *ipctl, unsigned int func_id)
{
	unsigned int value;

	value = readl_relaxed(ipctl->padctl_base + IO_PADSHARE_SEL_REG_REG);
	value &= 0x7;

	if (value <= IO_PADSHARE_SEL_REG_MAX){
		switch (value) {
			case 0:
				ipctl->padctl_gpio_base = IO_PAD_CTRL_SEL_FUNC_0_REG_ADDR;
				ipctl->padctl_gpio0 = PAD_GPIO_0;
				break;
			case 1:
				ipctl->padctl_gpio_base = IO_PAD_CTRL_SEL_FUNC_1_REG_ADDR;
				ipctl->padctl_gpio0 = PAD_GPIO_0;
				break;
			case 2:
				ipctl->padctl_gpio_base = IO_PAD_CTRL_SEL_FUNC_2_REG_ADDR;
				ipctl->padctl_gpio0 = PAD_FUNC_SHARE_72;
				break;
			case 3:
				ipctl->padctl_gpio_base = IO_PAD_CTRL_SEL_FUNC_3_REG_ADDR;
				ipctl->padctl_gpio0 = PAD_FUNC_SHARE_70;
				break;
			case 4: case 5: case 6:
				ipctl->padctl_gpio_base = IO_PAD_CTRL_SEL_FUNC_4_REG_ADDR;
				ipctl->padctl_gpio0 = PAD_FUNC_SHARE_0;
				break;
			default:
				dev_err(ipctl->dev, "invalid signal group %u\n", func_id);
				return -EINVAL;
		}
		return 0;
	}
	else{
		dev_err(ipctl->dev,"invalid signal group %u\n", func_id);
		return -EINVAL;
	}
}


static int starfive_vic7100_pmx_set_one_pin_mux(struct starfive_pinctrl *ipctl,
				    struct starfive_pin *pin)
{
	const struct starfive_pinctrl_soc_info *info = ipctl->info;
	struct starfive_pin_config *pin_config = &pin->pin_config;
	const struct starfive_pin_reg *pin_reg;
	unsigned int pin_id;
	u32 value;
	int i;
	unsigned long flags;
	
	pin_id = pin->pin;
	pin_reg = &ipctl->pin_regs[pin_id];
	
	raw_spin_lock_irqsave(&ipctl->lock, flags);
	if(pin_config->pinmux_func == PINMUX_GPIO_FUNC){
		if(pin_reg->gpo_dout_reg != -1){
			writel_relaxed(pin_config->gpio_dout, ipctl->gpio_base + pin_reg->gpo_dout_reg);
		}

		if(pin_reg->gpo_doen_reg != -1){
			writel_relaxed(pin_config->gpio_doen, ipctl->gpio_base + pin_reg->gpo_doen_reg);
		}

		for(i = 0; i < pin_config->gpio_din_num; i++){
			if((pin_config->gpio_din_reg[i] >= info->din_reg_base) &&
			  (pin_config->gpio_din_reg[i] <= info->din_reg_base + GPI_DIN_ADDR_END*info->din_reg_offset)){
				writel_relaxed((pin_config->gpio_num + 2), ipctl->gpio_base + pin_config->gpio_din_reg[i]);
			}
		}
	}
	raw_spin_unlock_irqrestore(&ipctl->lock, flags);
	
	return 0;
}


static const struct starfive_pinctrl_soc_info starfive_vic7100_pinctrl_info = {
	.pins = starfive_vic7100_pinctrl_pads,
	.npins = ARRAY_SIZE(starfive_vic7100_pinctrl_pads),
	.dout_reg_base = GPO_DOUT_CFG_BASE_REG,
	.dout_reg_offset = GPO_DOUT_CFG_REG_OFFSET,
	.doen_reg_base = GPO_DOEN_CFG_BASE_REG,
	.doen_reg_offset = GPO_DOEN_CFG_REG_OFFSET,
	.din_reg_base = GPI_DIN_CFG_BASE_REG,
	.din_reg_offset = GPI_DIN_CFG_REG_OFFSET,
	.starfive_iopad_sel_func = starfive_vic7100_iopad_sel_func,
	.starfive_pinconf_get = starfive_vic7100_pinconf_get,
	.starfive_pinconf_set = starfive_vic7100_pinconf_set,
	.starfive_pmx_set_one_pin_mux = starfive_vic7100_pmx_set_one_pin_mux,
	.starfive_gpio_register = starfive_vic7100_gpio_register,
};


static const struct of_device_id starfive_vic7100_pinctrl_of_match[] = {
	{ .compatible = "starfive_vic7100-pinctrl", .data = &starfive_vic7100_pinctrl_info, },
	{ /* sentinel */ }
};

static int starfive_vic7100_pinctrl_probe(struct platform_device *pdev)
{
	const struct starfive_pinctrl_soc_info *pinctrl_info;

	pinctrl_info = of_device_get_match_data(&pdev->dev);
	if (!pinctrl_info)
		return -ENODEV;
	

	return starfive_pinctrl_probe(pdev, pinctrl_info);
}

static struct platform_driver starfive_vic7100_pinctrl_driver = {
	.driver = {
		.name = "starfive_vic7100-pinctrl",
		.of_match_table = of_match_ptr(starfive_vic7100_pinctrl_of_match),
	},
	.probe = starfive_vic7100_pinctrl_probe,
};

static int __init starfive_vic7100_pinctrl_init(void)
{
	return platform_driver_register(&starfive_vic7100_pinctrl_driver);
}
arch_initcall(starfive_vic7100_pinctrl_init);
