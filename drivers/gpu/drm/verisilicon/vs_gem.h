/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 VeriSilicon Holdings Co., Ltd.
 */

#ifndef __VS_GEM_H__
#define __VS_GEM_H__

#include <linux/dma-buf.h>

#include <drm/drm_gem.h>
#include <drm/drm_gem_dma_helper.h>
#include <drm/drm_prime.h>

#include "vs_drv.h"
/*
 *
 * @base: drm_gem_dma_object.
 * @cookie: cookie returned by dma_alloc_attrs
 *	- not kernel virtual address with DMA_ATTR_NO_KERNEL_MAPPING
 * @dma_attrs: attribute for DMA API
 * @get_pages: flag for manually applying for non-contiguous memory.
 * @pages: Array of backing pages.
 *
 */
struct vs_gem_object {
	struct drm_gem_dma_object	base;
	void			*cookie;
	u32				iova;
	unsigned long	dma_attrs;
	bool			get_pages;
	struct page		**pages;
};

static inline struct vs_gem_object *
to_vs_gem_object(const struct drm_gem_object *bo)
{
	return container_of(to_drm_gem_dma_obj(bo), struct vs_gem_object, base);
}

int vs_gem_dumb_create(struct drm_file *file_priv,
		       struct drm_device *drm,
		       struct drm_mode_create_dumb *args);

struct drm_gem_object *
vs_gem_prime_import_sg_table(struct drm_device *dev,
			     struct dma_buf_attachment *attach,
			     struct sg_table *sgt);

#endif /* __VS_GEM_H__ */
