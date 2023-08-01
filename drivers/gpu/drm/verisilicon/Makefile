# SPDX-License-Identifier: GPL-2.0

vs_drm-objs := vs_dc_hw.o \
		vs_dc.o \
		vs_crtc.o \
		vs_drv.o \
		vs_modeset.o \
		vs_gem.o \
		vs_plane.o

vs_drm-$(CONFIG_STARFIVE_HDMI) += starfive_hdmi.o
obj-$(CONFIG_DRM_VERISILICON) += vs_drm.o

