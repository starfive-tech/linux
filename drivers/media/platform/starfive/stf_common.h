/* SPDX-License-Identifier: GPL-2.0 */
/*
 * stf_common.h
 *
 * StarFive Camera Subsystem - Common definitions
 *
 * Copyright (C) 2021-2023 StarFive Technology Co., Ltd.
 */

#ifndef STF_COMMON_H
#define STF_COMMON_H

#include <linux/io.h>
#include <linux/kern_levels.h>

enum {
	ST_DVP = 0x0001,
	ST_ISP = 0x0002,
	ST_VIN = 0x0004,
	ST_VIDEO = 0x0008,
	ST_CAMSS = 0x0010,
	ST_SENSOR = 0x0020,
};

enum {
	ST_NONE = 0x00,
	ST_ERR = 0x01,
	ST_WARN = 0x02,
	ST_INFO = 0x03,
	ST_DEBUG = 0x04,
};

#ifdef STF_DEBUG
#define STFDBG_LEVEL	ST_DEBUG
#define STFDBG_MASK	0x7F
#else
#define STFDBG_LEVEL	ST_ERR
#define STFDBG_MASK	0x7F
#endif

#define ST_MODULE2STRING(__module) ({ \
	char *__str;			\
					\
	switch (__module) {		\
	case ST_DVP:			\
		__str = "st_dvp";	\
		break;			\
	case ST_ISP:			\
		__str = "st_isp";	\
		break;			\
	case ST_VIN:			\
		__str = "st_vin";	\
		break;			\
	case ST_VIDEO:			\
		__str = "st_video";	\
		break;			\
	case ST_CAMSS:			\
		__str = "st_camss";	\
		break;			\
	case ST_SENSOR:			\
		__str = "st_sensor";	\
		break;			\
	default:			\
		__str = "unknown";	\
		break;			\
	}				\
					\
	__str;				\
	})

#define st_debug(module, __fmt, arg...)					\
	do {								\
		if (STFDBG_LEVEL > ST_INFO) {				\
			if (STFDBG_MASK & (module))			\
				pr_err("[%s] debug: " __fmt,		\
				       ST_MODULE2STRING((module)),	\
				       ## arg);				\
		}							\
	} while (0)

#define st_info(module, __fmt, arg...)					\
	do {								\
		if (STFDBG_LEVEL > ST_WARN) {				\
			if (STFDBG_MASK & (module))			\
				pr_err("[%s] info: " __fmt,		\
				       ST_MODULE2STRING((module)),	\
				       ## arg);				\
		}							\
	} while (0)

#define st_warn(module, __fmt, arg...)					\
	do {								\
		if (STFDBG_LEVEL > ST_ERR) {				\
			if (STFDBG_MASK & (module))			\
				pr_err("[%s] warn: " __fmt,		\
				       ST_MODULE2STRING((module)),	\
				       ## arg);				\
		}							\
	} while (0)

#define st_err(module, __fmt, arg...)					\
	do {								\
		if (STFDBG_LEVEL > ST_NONE) {				\
			if (STFDBG_MASK & (module))			\
				pr_err("[%s] error: " __fmt,		\
				       ST_MODULE2STRING((module)),	\
				       ## arg);				\
		}							\
	} while (0)

#define st_err_ratelimited(module, fmt, ...)				\
	do {								\
		static DEFINE_RATELIMIT_STATE(_rs,			\
					      DEFAULT_RATELIMIT_INTERVAL, \
					      DEFAULT_RATELIMIT_BURST); \
		if (__ratelimit(&_rs) && STFDBG_LEVEL > ST_NONE) {	\
			if (STFDBG_MASK & (module))			\
				pr_err("[%s] error: " fmt,		\
				       ST_MODULE2STRING((module)),	\
				       ##__VA_ARGS__);			\
		}							\
	} while (0)

static inline u32 reg_read(void __iomem *base, u32 reg)
{
	return ioread32(base + reg);
}

static inline void reg_write(void __iomem *base, u32 reg, u32 val)
{
	iowrite32(val, base + reg);
}

static inline void reg_set_bit(void __iomem *base, u32 reg, u32 mask, u32 val)
{
	u32 value;

	value = ioread32(base + reg) & ~mask;
	val &= mask;
	val |= value;
	iowrite32(val, base + reg);
}

static inline void reg_set(void __iomem *base, u32 reg, u32 mask)
{
	iowrite32(ioread32(base + reg) | mask, base + reg);
}

#endif /* STF_COMMON_H */
