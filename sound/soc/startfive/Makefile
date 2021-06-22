# starfive Platform Support
obj-$(CONFIG_SND_STARTFIVE_SPDIF) += spdif.o

spdif-y := startfive_spdif.o
spdif-$(CONFIG_SND_STARTFIVE_SPDIF_PCM) += startfive_spdif_pcm.o

obj-$(CONFIG_SND_STARTFIVE_PWMDAC) += pwmdac.o

pwmdac-y := startfive_pwmdac.o startfive_pwmdac_transmitter.o
pwmdac-$(CONFIG_SND_STARTFIVE_PWMDAC_PCM) += startfive_pwmdac_pcm.o

obj-$(CONFIG_SND_STARTFIVE_PDM) += startfive_pdm.o

obj-$(CONFIG_SND_STARTFIVE_I2SVAD) += i2svad.o
i2svad-y := startfive_i2svad.o
i2svad-$(CONFIG_SND_STARTFIVE_I2SVAD_PCM) += startfive_i2svad_pcm.o