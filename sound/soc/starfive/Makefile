# SPDX-License-Identifier: GPL-2.0
#
# Copyright (C) 2021 StarFive Technology Co., Ltd.
#
obj-$(CONFIG_SND_STARFIVE_SPDIF) += spdif.o

spdif-y := starfive_spdif.o
spdif-$(CONFIG_SND_STARFIVE_SPDIF_PCM) += starfive_spdif_pcm.o

obj-$(CONFIG_SND_STARFIVE_PWMDAC) += pwmdac.o

pwmdac-y := starfive_pwmdac.o starfive_pwmdac_transmitter.o
pwmdac-$(CONFIG_SND_STARFIVE_PWMDAC_PCM) += starfive_pwmdac_pcm.o

obj-$(CONFIG_SND_STARFIVE_PDM) += starfive_pdm.o

obj-$(CONFIG_SND_STARFIVE_I2SVAD) += i2svad.o
i2svad-y := starfive_i2svad.o
i2svad-$(CONFIG_SND_STARFIVE_I2SVAD_PCM) += starfive_i2svad_pcm.o