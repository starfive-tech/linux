# starfive Platform Support

obj-$(CONFIG_SND_SF_SOUND_CARD) += sf_sound.o

obj-$(CONFIG_SND_SF_MULTI_DAI) += sf_multi_dais.o 

obj-$(CONFIG_SND_SF_SPDIF) += spdif.o

spdif-y := sf_spdif.o
spdif-$(CONFIG_SND_SF_SPDIF_PCM) += sf_spdif_pcm.o

obj-$(CONFIG_SND_SF_PWMDAC) += pwmdac.o

pwmdac-y := sf_pwmdac.o sf_pwmdac_transmitter.o
pwmdac-$(CONFIG_SND_SF_PWMDAC_PCM) += sf_pwmdac_pcm.o

obj-$(CONFIG_SND_SF_PDM) += sf_pdm.o