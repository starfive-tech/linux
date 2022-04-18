#ifndef _DT_BINDINGS_VIC7100_PINCTRL_STARFIVE_H
#define _DT_BINDINGS_VIC7100_PINCTRL_STARFIVE_H

//gpo(n)_dout/doen signal pool  DOUT(signal pool)/DOEN(signal pool)
#define GPO_DOUT_LOW			0
#define GPO_DOUT_HIGH			1
#define GPO_DOEN_OUTPUT			0
#define GPO_DOEN_INPUT			1

#define GPO_CLK_GMAC_PAPHYREF		2
#define GPO_JTAG_TDO			3
#define GPO_JTAG_TDO_OEN		4
#define GPO_DMIC_CLK_OUT		5
#define GPO_DSP_JTDOEN			6
#define GPO_DSP_JTDO			7
#define GPO_I2C0_SCK_OE			8
#define GPO_I2C0_SDA_OE			9
#define GPO_I2C1_SCK_OE			10
#define GPO_I2C1_SDA_OE			11
#define GPO_I2C2_SCK_OE			12
#define GPO_I2C2_SDA_OE			13
#define GPO_I2C3_SCK_OE			14
#define GPO_I2C3_SDA_OE			15
#define GPO_I2SRX_BCLK_OUT		16
#define GPO_I2SRX_BCLK_OUT_OEN		17
#define GPO_I2SRX_LRCK_OUT		18
#define GPO_I2SRX_LRCK_OUT_OEN		19
#define GPO_I2SRX_MCLK_OUT		20
#define GPO_I2STX_BCLK_OUT		21
#define GPO_I2STX_BCLK_OUT_OEN		22
#define GPO_I2STX_LRCK_OUT		23
#define GPO_I2STX_LRCK_OUT_OEN		24
#define GPO_I2STX_MCLK_OUT		25
#define GPO_I2STX_SDOUT0		26
#define GPO_I2STX_SDOUT1		27
#define GPO_LCD_CSM_N			28
#define GPO_PWM_OE_N_BIT0		29
#define GPO_PWM_OE_N_BIT1		30
#define GPO_PWM_OE_N_BIT2		31
#define GPO_PWM_OE_N_BIT3		32
#define GPO_PWM_OE_N_BIT4		33
#define GPO_PWM_OE_N_BIT5		34
#define GPO_PWM_OE_N_BIT6		35
#define GPO_PWM_OE_N_BIT7		36
#define GPO_PWM_OUT_BIT0		37
#define GPO_PWM_OUT_BIT1		38
#define GPO_PWM_OUT_BIT2		39
#define GPO_PWM_OUT_BIT3		40
#define GPO_PWM_OUT_BIT4		41
#define GPO_PWM_OUT_BIT5		42
#define GPO_PWM_OUT_BIT6		43
#define GPO_PWM_OUT_BIT7		44
#define GPO_PWMDAC_LEFT_OUT		45
#define GPO_PWMDAC_RIGHT_OUT		46
#define GPO_QSPI_CSN1_OUT		47
#define GPO_QSPI_CSN2_OUT		48
#define GPO_QSPI_CSN3_OUT		49
#define GPO_REGISTER23_SCFG_CMSENSOR_RST0		50
#define GPO_REGISTER23_SCFG_CMSENSOR_RST1		51
#define GPO_REGISTER32_SCFG_GMAC_PHY_RSTN		52
#define GPO_SDIO0_CARD_POWER_EN		53
#define GPO_SDIO0_CCLK_OUT		54
#define GPO_SDIO0_CCMD_OE		55
#define GPO_SDIO0_CCMD_OUT		56
#define GPO_SDIO0_CDATA_OE_BIT0		57
#define GPO_SDIO0_CDATA_OE_BIT1		58
#define GPO_SDIO0_CDATA_OE_BIT2		59
#define GPO_SDIO0_CDATA_OE_BIT3		60
#define GPO_SDIO0_CDATA_OE_BIT4		61
#define GPO_SDIO0_CDATA_OE_BIT5		62
#define GPO_SDIO0_CDATA_OE_BIT6		63
#define GPO_SDIO0_CDATA_OE_BIT7		64
#define GPO_SDIO0_CDATA_OUT_BIT0	65
#define GPO_SDIO0_CDATA_OUT_BIT1	66
#define GPO_SDIO0_CDATA_OUT_BIT2	67
#define GPO_SDIO0_CDATA_OUT_BIT3	68
#define GPO_SDIO0_CDATA_OUT_BIT4	69
#define GPO_SDIO0_CDATA_OUT_BIT5	70
#define GPO_SDIO0_CDATA_OUT_BIT6	71
#define GPO_SDIO0_CDATA_OUT_BIT7	72
#define GPO_SDIO0_RST_N			73
#define GPO_SDIO1_CARD_POWER_EN		74
#define GPO_SDIO1_CCLK_OUT		75
#define GPO_SDIO1_CCMD_OE		76
#define GPO_SDIO1_CCMD_OUT		77
#define GPO_SDIO1_CDATA_OE_BIT0		78
#define GPO_SDIO1_CDATA_OE_BIT1		79
#define GPO_SDIO1_CDATA_OE_BIT2		80
#define GPO_SDIO1_CDATA_OE_BIT3		81
#define GPO_SDIO1_CDATA_OE_BIT4		82
#define GPO_SDIO1_CDATA_OE_BIT5		83
#define GPO_SDIO1_CDATA_OE_BIT6		84
#define GPO_SDIO1_CDATA_OE_BIT7		85
#define GPO_SDIO1_CDATA_OUT_BIT0	86
#define GPO_SDIO1_CDATA_OUT_BIT1	87
#define GPO_SDIO1_CDATA_OUT_BIT2	88
#define GPO_SDIO1_CDATA_OUT_BIT3	89
#define GPO_SDIO1_CDATA_OUT_BIT4	90
#define GPO_SDIO1_CDATA_OUT_BIT5	91
#define GPO_SDIO1_CDATA_OUT_BIT6	92
#define GPO_SDIO1_CDATA_OUT_BIT7	93
#define GPO_SDIO1_RST_N			94
#define GPO_SPDIF_TX_SDOUT		95
#define GPO_SPDIF_TX_SDOUT_OEN		96
#define GPO_SPI0_OE_N			97
#define GPO_SPI0_SCK_OUT		98
#define GPO_SPI0_SS_0_N			99
#define GPO_SPI0_SS_1_N			100
#define GPO_SPI0_TXD			101
#define GPO_SPI1_OE_N			102
#define GPO_SPI1_SCK_OUT		103
#define GPO_SPI1_SS_0_N			104
#define GPO_SPI1_SS_1_N			105
#define GPO_SPI1_TXD			106
#define GPO_SPI2_OE_N			107
#define GPO_SPI2_SCK_OUT		108
#define GPO_SPI2_SS_0_N			109
#define GPO_SPI2_SS_1_N			110
#define GPO_SPI2_TXD			111
#define GPO_SPI2AHB_OE_N_BIT0		112
#define GPO_SPI2AHB_OE_N_BIT1		113
#define GPO_SPI2AHB_OE_N_BIT2		114
#define GPO_SPI2AHB_OE_N_BIT3		115
#define GPO_SPI2AHB_TXD_BIT0		116
#define GPO_SPI2AHB_TXD_BIT1		117
#define GPO_SPI2AHB_TXD_BIT2		118
#define GPO_SPI2AHB_TXD_BIT3		119
#define GPO_SPI3_OE_N			120
#define GPO_SPI3_SCK_OUT		121
#define GPO_SPI3_SS_0_N			122
#define GPO_SPI3_SS_1_N			123
#define GPO_SPI3_TXD			124
#define GPO_UART0_DTRN			125
#define GPO_UART0_RTSN			126
#define GPO_UART0_SOUT			127
#define GPO_UART1_SOUT			128
#define GPO_UART2_DTR_N			129
#define GPO_UART2_RTS_N			130
#define GPO_UART2_SOUT			131
#define GPO_UART3_SOUT			132
#define GPO_USB_DRV_BUS			133


//gpi(n)signal pool offset address   DIN(signal pool offset address)
#define GPI_CPU_JTAG_TCK		0
#define GPI_CPU_JTAG_TDI		1
#define GPI_CPU_JTAG_TMS		2
#define GPI_CPU_JTAG_TRST		3
#define GPI_DMIC_SDIN_BIT0		4
#define GPI_DMIC_SDIN_BIT1		5
#define GPI_DSP_JTCK			6
#define GPI_DSP_JTDI			7
#define GPI_DSP_JTMS			8
#define GPI_DSP_TRST			9
#define GPI_I2C0_SCK_IN			10
#define GPI_I2C0_SDA_IN			11
#define GPI_I2C1_SCK_IN			12
#define GPI_I2C1_SDA_IN			13
#define GPI_I2C2_SCK_IN			14
#define GPI_I2C2_SDA_IN			15
#define GPI_I2C3_SCK_IN			16
#define GPI_I2C3_SDA_IN			17
#define GPI_I2SRX_BCLK_IN		18
#define GPI_I2SRX_LRCK_IN		19
#define GPI_I2SRX_SDIN_BIT0		20
#define GPI_I2SRX_SDIN_BIT1		21
#define GPI_I2SRX_SDIN_BIT2		22
#define GPI_I2STX_BCLK_IN		23
#define GPI_I2STX_LRCK_IN		24
#define GPI_SDIO0_CARD_DETECT_N		25
#define GPI_SDIO0_CARD_WRITE_PRT	26
#define GPI_SDIO0_CCMD_IN		27
#define GPI_SDIO0_CDATA_IN_BIT0		28
#define GPI_SDIO0_CDATA_IN_BIT1		29
#define GPI_SDIO0_CDATA_IN_BIT2		30
#define GPI_SDIO0_CDATA_IN_BIT3		31
#define GPI_SDIO0_CDATA_IN_BIT4		32
#define GPI_SDIO0_CDATA_IN_BIT5		33
#define GPI_SDIO0_CDATA_IN_BIT6		34
#define GPI_SDIO0_CDATA_IN_BIT7		35
#define GPI_SDIO1_CARD_DETECT_N		36
#define GPI_SDIO1_CARD_WRITE_PRT	37
#define GPI_SDIO1_CCMD_IN		38
#define GPI_SDIO1_CDATA_IN_BIT0		39
#define GPI_SDIO1_CDATA_IN_BIT1		40
#define GPI_SDIO1_CDATA_IN_BIT2		41
#define GPI_SDIO1_CDATA_IN_BIT3		42
#define GPI_SDIO1_CDATA_IN_BIT4		43
#define GPI_SDIO1_CDATA_IN_BIT5		44
#define GPI_SDIO1_CDATA_IN_BIT6		45
#define GPI_SDIO1_CDATA_IN_BIT7		46
#define GPI_SPDIF_RX_SDIN		47
#define GPI_SPI0_RXD			48
#define GPI_SPI0_SS_IN_N		49
#define GPI_SPI1_RXD			50
#define GPI_SPI1_SS_IN_N		51
#define GPI_SPI2_RXD			52
#define GPI_SPI2_SS_IN_N		53
#define GPI_SPI2AHB_RXD_BIT0		54
#define GPI_SPI2AHB_RXD_BIT1		55
#define GPI_SPI2AHB_RXD_BIT2		56
#define GPI_SPI2AHB_RXD_BIT3		57
#define GPI_SPI2AHB_SS_N		58
#define GPI_SPI2AHB_SLV_SCLKIN		59
#define GPI_SPI3_RXD			60
#define GPI_SPI3_SS_IN_N		61
#define GPI_UART0_CTSN			62
#define GPI_UART0_DCDN			63
#define GPI_UART0_DSRN			64
#define GPI_UART0_RIN			65
#define GPI_UART0_SIN			66
#define GPI_UART1_SIN			67
#define GPI_UART2_CTS_N			68
#define GPI_UART2_DCD_N			69
#define GPI_UART2_DSR_N			70
#define GPI_UART2_RI_N			71
#define GPI_UART2_SIN			72
#define GPI_UART3_SIN			73
#define GPI_USB_OVER_CURRENT		74


#define PAD_GPIO_MAX			64

//pins num rage(0-205) :PAD_GPIO(0~63) PAD_FUNC_SHARE(0~141)
#define PAD_GPIO(num)			(num)
#define PAD_FUNC_SHARE(num)		(PAD_GPIO_MAX + num)

//pinmux
#define GPIO(num)			(num & 0xFF)

/* io pad control config */
#define GPIO_DS(data)			((data << 0x0U) & 0xFU) /*driving strength[3:0]*/
#define GPIO_PD(data)			((data << 0x4U) & 0x10U) /*pulldown enable[4]*/
#define GPIO_PU(data)			((data << 0x5U) & 0x20U) /*pullup enable[5]*/
#define GPIO_SMT(data)			((data << 0x6U) & 0x40U) /*schemit input enable[6]*/
#define GPIO_IE(data)			((data << 0x7U) & 0x80U) /*input enable[7]*/
#define GPIO_POS(data)			((data << 0x8U) & 0x100U) /*strength pullup enable[8]*/
#define GPIO_SLEW(data)			((data << 0x9U) & 0xE00U) /*slew control[11:9]*/

#define DO_REVERSE			(0x1)
#define IO(config)			(config & 0xFFFF)
#define DOUT(dout,reverse)		((dout & 0xFF) | (reverse << 31))
#define DOEN(doen,reverse)		((doen & 0xFF) | (reverse << 31))
#define DIN(din_reg)			(din_reg & 0xFF)


#endif //_DT_BINDINGS_VIC7100_PINCTRL_STARFIVE_H
