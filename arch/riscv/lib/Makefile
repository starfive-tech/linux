# SPDX-License-Identifier: GPL-2.0-only
lib-y			+= delay.o
lib-y			+= memset.o
lib-y			+= memmove.o
lib-$(CONFIG_MMU)	+= uaccess.o
lib-$(CONFIG_64BIT)	+= tishift.o
lib-$(CONFIG_CC_OPTIMIZE_FOR_PERFORMANCE) += string.o

obj-$(CONFIG_FUNCTION_ERROR_INJECTION) += error-inject.o
