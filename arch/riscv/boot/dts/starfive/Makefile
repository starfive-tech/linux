# SPDX-License-Identifier: GPL-2.0
# Enables support for device-tree overlays
DTC_FLAGS_jh7100-beaglev-starlight := -@
DTC_FLAGS_jh7100-starfive-visionfive-v1 := -@
DTC_FLAGS_jh7110-starfive-visionfive-2-v1.2a := -@
DTC_FLAGS_jh7110-starfive-visionfive-2-v1.3b := -@
DTC_FLAGS_jh7110-evb := -@

dtb-$(CONFIG_ARCH_STARFIVE) += jh7100-beaglev-starlight.dtb
dtb-$(CONFIG_ARCH_STARFIVE) += jh7100-starfive-visionfive-v1.dtb

dtb-$(CONFIG_ARCH_STARFIVE) += jh7110-starfive-visionfive-2-v1.2a.dtb
dtb-$(CONFIG_ARCH_STARFIVE) += jh7110-starfive-visionfive-2-v1.3b.dtb	\
						jh7110-starfive-visionfive-2-ac108.dtb	\
						jh7110-starfive-visionfive-2-tdm.dtb	\
						jh7110-starfive-visionfive-2-wm8960.dtb

subdir-y += evb-overlay
dtb-$(CONFIG_ARCH_STARFIVE) += jh7110-evb.dtb			\
			       jh7110-evb-pcie-i2s-sd.dtb	\
			       jh7110-evb-spi-uart2.dtb		\
			       jh7110-evb-uart4-emmc-spdif.dtb	\
			       jh7110-evb-uart5-pwm-i2c-tdm.dtb	\
			       jh7110-evb-dvp-rgb2hdmi.dtb	\
			       jh7110-evb-can-pdm-pwmdac.dtb	\
			       jh7110-evb-i2s-ac108.dtb		\
			       jh7110-evb-usbdevice.dtb		\
			       jh7110-evb-uart1-rgb2hdmi.dtb
