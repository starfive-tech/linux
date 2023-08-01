// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 VeriSilicon Holdings Co., Ltd.
 */

#include <linux/dma-buf.h>
#include <linux/of_reserved_mem.h>
#include <drm/drm_gem_dma_helper.h>

#include "vs_drv.h"
#include "vs_gem.h"

MODULE_IMPORT_NS(DMA_BUF);

static const struct drm_gem_object_funcs vs_gem_default_funcs;

static int vs_gem_alloc_buf(struct vs_gem_object *vs_obj)
{
	struct drm_device *dev = vs_obj->base.base.dev;
	unsigned int nr_pages;
	struct sg_table sgt;
	int ret = -ENOMEM;

	if (vs_obj->base.dma_addr) {
		drm_dbg_kms(dev, "already allocated.\n");
		return 0;
	}

	vs_obj->base.dma_addr = DMA_ATTR_WRITE_COMBINE | DMA_ATTR_FORCE_CONTIGUOUS
			   | DMA_ATTR_NO_KERNEL_MAPPING;

	nr_pages = vs_obj->base.base.size >> PAGE_SHIFT;

	vs_obj->pages = kvmalloc_array(nr_pages, sizeof(struct page *),
				       GFP_KERNEL | __GFP_ZERO);
	if (!vs_obj->pages)
		return -ENOMEM;

	vs_obj->cookie = dma_alloc_attrs(dev->dev, vs_obj->base.base.size,
					 &vs_obj->base.dma_addr, GFP_KERNEL,
					 vs_obj->dma_attrs);

	if (!vs_obj->cookie) {
		dev_err(dev->dev, "failed to allocate buffer.\n");
		goto err_free;
	}

	vs_obj->iova = vs_obj->base.dma_addr;

	ret = dma_get_sgtable_attrs(dev->dev, &sgt,
				    vs_obj->cookie, vs_obj->base.dma_addr,
				    vs_obj->base.base.size, vs_obj->dma_attrs);
	if (ret < 0) {
		dev_err(dev->dev, "failed to get sgtable.\n");
		goto err_mem_free;
	}

	if (drm_prime_sg_to_page_array(&sgt, vs_obj->pages, nr_pages)) {
		dev_err(dev->dev, "invalid sgtable.\n");
		ret = -EINVAL;
		goto err_sgt_free;
	}

	sg_free_table(&sgt);

	return 0;

err_sgt_free:
	sg_free_table(&sgt);
err_mem_free:
		dma_free_attrs(dev->dev, vs_obj->base.base.size, vs_obj->cookie,
			       vs_obj->base.dma_addr, vs_obj->dma_attrs);
err_free:
	kvfree(vs_obj->pages);

	return ret;
}

static void vs_gem_free_buf(struct vs_gem_object *vs_obj)
{
	struct drm_device *dev = vs_obj->base.base.dev;

	if (!vs_obj->base.dma_addr) {
		drm_dbg_kms(dev, "dma_addr is invalid.\n");
		return;
	}

	dma_free_attrs(dev->dev, vs_obj->base.base.size, vs_obj->cookie,
		       (dma_addr_t)vs_obj->base.dma_addr,
		       vs_obj->dma_attrs);

	kvfree(vs_obj->pages);
}

static void vs_gem_free_object(struct drm_gem_object *obj)
{
	struct vs_gem_object *vs_obj = to_vs_gem_object(obj);

	if (obj->import_attach)
		drm_prime_gem_destroy(obj, vs_obj->base.sgt);
	else
		vs_gem_free_buf(vs_obj);

	drm_gem_object_release(obj);

	kfree(vs_obj);
}

static struct vs_gem_object *vs_gem_alloc_object(struct drm_device *dev,
						 size_t size)
{
	struct vs_gem_object *vs_obj;
	struct drm_gem_object *obj;
	int ret;

	vs_obj = kzalloc(sizeof(*vs_obj), GFP_KERNEL);
	if (!vs_obj)
		return ERR_PTR(-ENOMEM);

	vs_obj->base.base.size = size;
	obj = &vs_obj->base.base;

	ret = drm_gem_object_init(dev, obj, size);
	if (ret)
		goto err_free;

	vs_obj->base.base.funcs = &vs_gem_default_funcs;

	ret = drm_gem_create_mmap_offset(obj);
	if (ret) {
		drm_gem_object_release(obj);
		goto err_free;
	}

	return vs_obj;

err_free:
	kfree(vs_obj);
	return ERR_PTR(ret);
}

static struct vs_gem_object *vs_gem_create_object(struct drm_device *dev,
						  size_t size)
{
	struct vs_gem_object *vs_obj;
	int ret;

	size = PAGE_ALIGN(size);

	vs_obj = vs_gem_alloc_object(dev, size);
	if (IS_ERR(vs_obj))
		return vs_obj;

	ret = vs_gem_alloc_buf(vs_obj);
	if (ret) {
		drm_gem_object_release(&vs_obj->base.base);
		kfree(vs_obj);
		return ERR_PTR(ret);
	}

	return vs_obj;
}

static struct vs_gem_object *vs_gem_create_with_handle(struct drm_device *dev,
						       struct drm_file *file,
						       size_t size,
						       unsigned int *handle)
{
	struct vs_gem_object *vs_obj;
	struct drm_gem_object *obj;
	int ret;

	vs_obj = vs_gem_create_object(dev, size);
	if (IS_ERR(vs_obj))
		return vs_obj;

	obj = &vs_obj->base.base;

	ret = drm_gem_handle_create(file, obj, handle);

	drm_gem_object_put(obj);

	if (ret)
		return ERR_PTR(ret);

	return vs_obj;
}

static struct sg_table *vs_gem_prime_get_sg_table(struct drm_gem_object *obj)
{
	struct vs_gem_object *vs_obj = to_vs_gem_object(obj);

	return drm_prime_pages_to_sg(obj->dev, vs_obj->pages,
					 vs_obj->base.base.size >> PAGE_SHIFT);
}

static int vs_gem_prime_vmap(struct drm_gem_object *obj, struct iosys_map *map)
{
	struct vs_gem_object *vs_obj = to_vs_gem_object(obj);

	vs_obj->base.vaddr = vs_obj->dma_attrs & DMA_ATTR_NO_KERNEL_MAPPING ?
		       page_address(vs_obj->cookie) : vs_obj->cookie;

	return drm_gem_dma_vmap(&vs_obj->base, map);
}

static const struct vm_operations_struct vs_vm_ops = {
	.open  = drm_gem_vm_open,
	.close = drm_gem_vm_close,
};

static const struct drm_gem_object_funcs vs_gem_default_funcs = {
	.free = vs_gem_free_object,
	.get_sg_table = vs_gem_prime_get_sg_table,
	.vmap = vs_gem_prime_vmap,
	.mmap = drm_gem_dma_object_mmap,
	.vm_ops = &vs_vm_ops,
};

int vs_gem_dumb_create(struct drm_file *file,
		       struct drm_device *dev,
		       struct drm_mode_create_dumb *args)
{
	struct vs_drm_private *priv = to_vs_dev(dev);
	struct vs_gem_object *vs_obj;
	unsigned int pitch = DIV_ROUND_UP(args->width * args->bpp, 8);

	if (args->bpp % 10)
		args->pitch = ALIGN(pitch, priv->pitch_alignment);
	else
		/* for costum 10bit format with no bit gaps */
		args->pitch = pitch;
	args->size = PAGE_ALIGN(args->pitch * args->height);
	vs_obj = vs_gem_create_with_handle(dev, file, args->size,
					   &args->handle);
	return PTR_ERR_OR_ZERO(vs_obj);
}

struct drm_gem_object *
vs_gem_prime_import_sg_table(struct drm_device *dev,
			     struct dma_buf_attachment *attach,
			     struct sg_table *sgt)
{
	struct vs_gem_object *vs_obj;
	int npages;
	int ret;
	struct scatterlist *s;
	u32 i;
	dma_addr_t expected;
	size_t size = attach->dmabuf->size;

	size = PAGE_ALIGN(size);

	vs_obj = vs_gem_alloc_object(dev, size);
	if (IS_ERR(vs_obj))
		return ERR_CAST(vs_obj);

	expected = sg_dma_address(sgt->sgl);
	for_each_sg(sgt->sgl, s, sgt->nents, i) {
		if (sg_dma_address(s) != expected) {
			DRM_ERROR("sg_table is not contiguous");
			ret = -EINVAL;
			goto err;
		}
		if (sg_dma_len(s) & (PAGE_SIZE - 1)) {
			ret = -EINVAL;
			goto err;
		}
		if (i == 0)
			vs_obj->iova = sg_dma_address(s);
		expected = sg_dma_address(s) + sg_dma_len(s);
	}

	vs_obj->base.dma_addr = sg_dma_address(sgt->sgl);

	npages = vs_obj->base.base.size >> PAGE_SHIFT;
	vs_obj->pages = kvmalloc_array(npages, sizeof(struct page *),
				       GFP_KERNEL);
	if (!vs_obj->pages) {
		ret = -ENOMEM;
		goto err;
	}

	ret = drm_prime_sg_to_page_array(sgt, vs_obj->pages, npages);
	if (ret)
		goto err_free_page;

	vs_obj->base.sgt = sgt;

	return &vs_obj->base.base;

err_free_page:
	kvfree(vs_obj->pages);
err:
	vs_gem_free_object(&vs_obj->base.base);

	return ERR_PTR(ret);
}
