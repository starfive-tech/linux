/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021 StarFive Technology Co., Ltd
 * Author: StarFive <StarFive@starfivetech.com>
 */

#ifndef _STARFIVE_DRM_GEM_H
#define _STARFIVE_DRM_GEM_H

#include <drm/drm_gem.h>

struct starfive_drm_gem_obj {
	struct drm_gem_object	base;
	//void			*cookie; //mtk
	void			*kvaddr;
	dma_addr_t		dma_addr;
	unsigned long		dma_attrs;

	/* Used when IOMMU is enabled */
	unsigned long	num_pages;
	struct sg_table		*sg;
	struct page		**pages;
};
#define to_starfive_gem_obj(x)	container_of(x, struct starfive_drm_gem_obj, base)


void starfive_drm_gem_free_object(struct drm_gem_object *obj);
int starfive_drm_gem_mmap(struct file *filp, struct vm_area_struct *vma);
int starfive_drm_gem_mmap_buf(struct drm_gem_object *obj,
			  struct vm_area_struct *vma);
int starfive_drm_gem_dumb_create(struct drm_file *file_priv,
			     struct drm_device *dev,
			     struct drm_mode_create_dumb *args);
struct sg_table *starfive_drm_gem_prime_get_sg_table(struct drm_gem_object *obj);
struct drm_gem_object *
starfive_drm_gem_prime_import_sg_table(struct drm_device *dev,
				   struct dma_buf_attachment *attach,
				   struct sg_table *sg);
void *starfive_drm_gem_prime_vmap(struct drm_gem_object *obj);
void starfive_drm_gem_prime_vunmap(struct drm_gem_object *obj, void *vaddr);
#endif /* _STARFIVE_DRM_GEM_H */
