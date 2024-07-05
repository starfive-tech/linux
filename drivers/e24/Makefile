# SPDX-License-Identifier: GPL-2.0
# Copyright(c) 1999 - 2018 Intel Corporation.
#
# Makefile for the E24 driver
#
ccflags-y += -I$(srctree)/drivers/e24
#ccflags-y += -DDEBUG
ccflags-y += -Wunused-variable

obj-$(CONFIG_E24) += e24.o

e24-y := starfive_e24.o starfive_e24_hw.o e24_alloc.o
