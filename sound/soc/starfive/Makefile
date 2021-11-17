# SPDX-License-Identifier: GPL-2.0
#
# Copyright (C) 2021 StarFive Technology Co., Ltd.
#
snd-soc-starfive-spdif-y := spdif.o
snd-soc-starfive-spdif-$(CONFIG_SND_STARFIVE_SPDIF_PCM) += spdif-pcm.o

obj-$(CONFIG_SND_STARFIVE_SPDIF) += snd-soc-starfive-spdif.o

snd-soc-starfive-pwmdac-y := pwmdac.o
snd-soc-starfive-pwmdac-$(CONFIG_SND_STARFIVE_PWMDAC_PCM) += pwmdac-pcm.o
snd-soc-starfive-pwmdac-transmitter-y := pwmdac-transmitter.o

obj-$(CONFIG_SND_STARFIVE_PWMDAC) += snd-soc-starfive-pwmdac.o
obj-$(CONFIG_SND_STARFIVE_PWMDAC) += snd-soc-starfive-pwmdac-transmitter.o

snd-soc-starfive-pdm-y := pdm.o

obj-$(CONFIG_SND_STARFIVE_PDM) += snd-soc-starfive-pdm.o

snd-soc-starfive-i2svad-y := i2svad.o
snd-soc-starfive-i2svad-$(CONFIG_SND_STARFIVE_I2SVAD_PCM) += i2svad-pcm.o

obj-$(CONFIG_SND_STARFIVE_I2SVAD) += snd-soc-starfive-i2svad.o
