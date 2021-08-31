// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 StarFive Technology Co., Ltd.
 */
#include <linux/dma-buf.h>
#include <linux/iommu.h>
#include <linux/vmalloc.h>
#include <drm/drm.h>
#include <drm/drm_gem.h>
#include <drm/drm_prime.h>
#include <drm/drm_vma_manager.h>
#include <drm/drm_gem_cma_helper.h>
#include "starfive_drm_drv.h"
#include "starfive_drm_gem.h"

static const struct drm_gem_object_funcs starfive_gem_object_funcs;
static const struct vm_operations_struct mmap_mem_ops = {
#ifdef CONFIG_HAVE_IOREMAP_PROT
	.access = generic_access_phys
#endif
};

static int starfive_drm_gem_object_mmap_dma(struct drm_gem_object *obj,
					    struct vm_area_struct *vma)
{
	struct starfive_drm_gem_obj *starfive_obj = to_starfive_gem_obj(obj);
	struct drm_device *drm = obj->dev;

	return dma_mmap_attrs(drm->dev, vma, starfive_obj->kvaddr,
			starfive_obj->dma_addr, obj->size, starfive_obj->dma_attrs);
}

static int starfive_drm_gem_object_mmap(struct drm_gem_object *obj,
					struct vm_area_struct *vma)
{
	int ret;

	/*
	 * We allocated a struct page table for rk_obj, so clear
	 * VM_PFNMAP flag that was set by drm_gem_mmap_obj()/drm_gem_mmap().
	 */
	vma->vm_flags &= ~VM_PFNMAP;
	ret = starfive_drm_gem_object_mmap_dma(obj, vma);
	if (ret)
		drm_gem_vm_close(vma);

	return ret;
}

int starfive_drm_gem_mmap_buf(struct drm_gem_object *obj,
			      struct vm_area_struct *vma)
{
	int ret = drm_gem_mmap_obj(obj, obj->size, vma);

	if (ret)
		return ret;

	return starfive_drm_gem_object_mmap(obj, vma);
}

int starfive_drm_gem_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct drm_gem_object *obj;
	int ret;

	ret = drm_gem_mmap(filp, vma);
	if (ret)
		return ret;

	obj = vma->vm_private_data;

	/*
	 * Set vm_pgoff (used as a fake buffer offset by DRM) to 0 and map the
	 * whole buffer from the start.
	 */
	vma->vm_pgoff = 0;

	return starfive_drm_gem_object_mmap(obj, vma);
}

void starfive_drm_gem_free_object(struct drm_gem_object *obj)
{
	struct starfive_drm_gem_obj *starfive_gem = to_starfive_gem_obj(obj);
	struct drm_device *drm_dev = obj->dev;

	if (starfive_gem->sg)
		drm_prime_gem_destroy(obj, starfive_gem->sg);
	else
		dma_free_attrs(drm_dev->dev, obj->size, starfive_gem->kvaddr,
			       starfive_gem->dma_addr, starfive_gem->dma_attrs);

	/* release file pointer to gem object. */
	drm_gem_object_release(obj);

	kfree(starfive_gem);
}

static struct starfive_drm_gem_obj *
starfive_drm_gem_alloc_object(struct drm_device *drm, unsigned int size)
{
	struct starfive_drm_gem_obj *starfive_obj;
	struct drm_gem_object *obj;
	int ret;

	starfive_obj = kzalloc(sizeof(*starfive_obj), GFP_KERNEL);
	if (!starfive_obj)
		return ERR_PTR(-ENOMEM);

	obj = &starfive_obj->base;
	ret = drm_gem_object_init(drm, obj, round_up(size, PAGE_SIZE));
	if (ret)
		return ERR_PTR(ret);

	return starfive_obj;
}

static int starfive_drm_gem_alloc_dma(struct starfive_drm_gem_obj *starfive_obj,
				      bool alloc_kmap)
{
	struct drm_gem_object *obj = &starfive_obj->base;
	struct drm_device *drm = obj->dev;

	starfive_obj->dma_attrs = DMA_ATTR_WRITE_COMBINE;
	if (!alloc_kmap)
		starfive_obj->dma_attrs |= DMA_ATTR_NO_KERNEL_MAPPING;

	starfive_obj->kvaddr = dma_alloc_attrs(drm->dev, obj->size,
					       &starfive_obj->dma_addr, GFP_KERNEL,
					       starfive_obj->dma_attrs);

	DRM_INFO("kvaddr = 0x%px\n", starfive_obj->kvaddr);
	DRM_INFO("dma_addr = 0x%llx, size = %lu\n", starfive_obj->dma_addr, obj->size);
	if (!starfive_obj->kvaddr) {
		DRM_ERROR("failed to allocate %zu byte dma buffer", obj->size);
		return -ENOMEM;
	}

	return 0;
}

static int starfive_drm_gem_alloc_buf(struct starfive_drm_gem_obj *starfive_obj,
				      bool alloc_kmap)
{
	return starfive_drm_gem_alloc_dma(starfive_obj, alloc_kmap);
}

static void starfive_drm_gem_release_object(struct starfive_drm_gem_obj *starfive_obj)
{
	drm_gem_object_release(&starfive_obj->base);
	kfree(starfive_obj);
}

static struct starfive_drm_gem_obj *
starfive_drm_gem_create_object(struct drm_device *drm, unsigned int size,
			       bool alloc_kmap)
{
	struct starfive_drm_gem_obj *starfive_obj;
	int ret;

	starfive_obj = starfive_drm_gem_alloc_object(drm, size);
	if (IS_ERR(starfive_obj))
		return starfive_obj;

	ret = starfive_drm_gem_alloc_buf(starfive_obj, alloc_kmap);
	if (ret)
		goto err_free_obj;

	starfive_obj->base.funcs = &starfive_gem_object_funcs;

	return starfive_obj;

err_free_obj:
	starfive_drm_gem_release_object(starfive_obj);
	return ERR_PTR(ret);

}

static struct starfive_drm_gem_obj *
starfive_drm_gem_create_with_handle(struct drm_file *file_priv,
				    struct drm_device *drm,
				    unsigned int size,
				    unsigned int *handle)
{
	struct starfive_drm_gem_obj *starfive_gem;
	struct drm_gem_object *gem;
	int ret;
#ifdef CONFIG_FRAMEBUFFER_CONSOLE
	//config true, for console display
	starfive_gem = starfive_drm_gem_create_object(drm, size, true);
#else
	starfive_gem = starfive_drm_gem_create_object(drm, size, false);
#endif
	if (IS_ERR(starfive_gem))
		return ERR_CAST(starfive_gem);

	gem = &starfive_gem->base;

	/*
	 * allocate a id of idr table where the obj is registered
	 * and handle has the id what user can see.
	 */
	ret = drm_gem_handle_create(file_priv, gem, handle);
	if (ret)
		goto err_handle_create;

	/* drop reference from allocate - handle holds it now. */
	drm_gem_object_put(gem);

	return starfive_gem;

err_handle_create:
	starfive_drm_gem_free_object(gem);

	return ERR_PTR(ret);
}

int starfive_drm_gem_dumb_create(struct drm_file *file_priv,
				 struct drm_device *dev,
				 struct drm_mode_create_dumb *args)
{
	struct starfive_drm_gem_obj *starfive_gem;

	args->pitch = DIV_ROUND_UP(args->width * args->bpp, 8);
	args->size = args->pitch * args->height;

	starfive_gem = starfive_drm_gem_create_with_handle(file_priv, dev,
							   args->size,
							   &args->handle);

	return PTR_ERR_OR_ZERO(starfive_gem);
}

struct sg_table *starfive_drm_gem_prime_get_sg_table(struct drm_gem_object *obj)
{
	struct starfive_drm_gem_obj *starfive_obj = to_starfive_gem_obj(obj);
	struct drm_device *drm = obj->dev;
	struct sg_table *sgt;
	int ret;

	if (starfive_obj->pages)
		return drm_prime_pages_to_sg(obj->dev, starfive_obj->pages,
					     starfive_obj->num_pages);

	sgt = kzalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt)
		return ERR_PTR(-ENOMEM);

	ret = dma_get_sgtable_attrs(drm->dev, sgt, starfive_obj->kvaddr,
				    starfive_obj->dma_addr, obj->size,
				    starfive_obj->dma_attrs);
	if (ret) {
		DRM_ERROR("failed to allocate sgt, %d\n", ret);
		kfree(sgt);
		return ERR_PTR(ret);
	}

	return sgt;
}

static int
starfive_drm_gem_dma_map_sg(struct drm_device *drm,
			    struct dma_buf_attachment *attach,
			    struct sg_table *sg,
			    struct starfive_drm_gem_obj *starfive_obj)
{
	int err;

	err = dma_map_sgtable(drm->dev, sg, DMA_BIDIRECTIONAL, 0);
	if (err)
		return err;

	if (drm_prime_get_contiguous_size(sg) < attach->dmabuf->size) {
		DRM_ERROR("failed to map sg_table to contiguous linear address.\n");
		dma_unmap_sgtable(drm->dev, sg, DMA_BIDIRECTIONAL, 0);
		return -EINVAL;
	}

	starfive_obj->dma_addr = sg_dma_address(sg->sgl);
	starfive_obj->sg = sg;

	return 0;
}

struct drm_gem_object *
starfive_drm_gem_prime_import_sg_table(struct drm_device *drm,
				       struct dma_buf_attachment *attach,
				       struct sg_table *sg)
{
	struct starfive_drm_gem_obj *starfive_obj;
	int ret;

	starfive_obj = starfive_drm_gem_alloc_object(drm, attach->dmabuf->size);
	if (IS_ERR(starfive_obj))
		return ERR_CAST(starfive_obj);

	ret = starfive_drm_gem_dma_map_sg(drm, attach, sg, starfive_obj);
	if (ret < 0) {
		DRM_ERROR("failed to import sg table: %d\n", ret);
		goto err_free_obj;
	}

	return &starfive_obj->base;

err_free_obj:
	starfive_drm_gem_release_object(starfive_obj);
	return ERR_PTR(ret);
}

int starfive_drm_gem_prime_vmap(struct drm_gem_object *obj, struct iosys_map *map)
{
	struct starfive_drm_gem_obj *starfive_obj = to_starfive_gem_obj(obj);

	if (starfive_obj->pages) {
		void *vaddr = vmap(starfive_obj->pages, starfive_obj->num_pages, VM_MAP,
						pgprot_writecombine(PAGE_KERNEL));
		if (!vaddr)
			return -ENOMEM;
		iosys_map_set_vaddr(map, vaddr);
		return 0;
	}

	if (starfive_obj->dma_attrs & DMA_ATTR_NO_KERNEL_MAPPING)
		return -ENOMEM;

	iosys_map_set_vaddr(map, starfive_obj->kvaddr);

	return 0;
}


void starfive_drm_gem_prime_vunmap(struct drm_gem_object *obj, struct iosys_map *map)
{
	struct starfive_drm_gem_obj *starfive_obj = to_starfive_gem_obj(obj);

	if (starfive_obj->pages) {
		vunmap(map->vaddr);
		return;
	}
	/* Nothing to do if allocated by DMA mapping API. */
}

static const struct drm_gem_object_funcs starfive_gem_object_funcs = {
	.free		= starfive_drm_gem_free_object,
	.get_sg_table	= starfive_drm_gem_prime_get_sg_table,
	.vmap		= starfive_drm_gem_prime_vmap,
	.vunmap		= starfive_drm_gem_prime_vunmap,
	.vm_ops		= &drm_gem_cma_vm_ops,
};
