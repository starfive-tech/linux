// SPDX-License-Identifier: GPL-2.0
#include <linux/atomic.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include "e24_alloc.h"

struct e24_private_pool {
	struct e24_allocation_pool pool;
	struct mutex free_list_lock;
	phys_addr_t start;
	u32 size;
	struct e24_allocation *free_list;
};

static void e24_private_free(struct e24_allocation *e24_allocation)
{
	struct e24_private_pool *pool = container_of(e24_allocation->pool,
						     struct e24_private_pool,
						     pool);
	struct e24_allocation **pcur;

	pr_debug("%s: %pap x %d\n", __func__,
		 &e24_allocation->start, e24_allocation->size);

	mutex_lock(&pool->free_list_lock);

	for (pcur = &pool->free_list; ; pcur = &(*pcur)->next) {
		struct e24_allocation *cur = *pcur;

		if (cur && cur->start + cur->size == e24_allocation->start) {
			struct e24_allocation *next = cur->next;

			pr_debug("merging block tail: %pap x 0x%x ->\n",
				 &cur->start, cur->size);
			cur->size += e24_allocation->size;
			pr_debug("... -> %pap x 0x%x\n",
				 &cur->start, cur->size);
			kfree(e24_allocation);

			if (next && cur->start + cur->size == next->start) {
				pr_debug("merging with next block: %pap x 0x%x ->\n",
					 &cur->start, cur->size);
				cur->size += next->size;
				cur->next = next->next;
				pr_debug("... -> %pap x 0x%x\n",
					 &cur->start, cur->size);
				kfree(next);
			}
			break;
		}

		if (!cur || e24_allocation->start < cur->start) {
			if (cur && e24_allocation->start + e24_allocation->size ==
			    cur->start) {
				pr_debug("merging block head: %pap x 0x%x ->\n",
					 &cur->start, cur->size);
				cur->size += e24_allocation->size;
				cur->start = e24_allocation->start;
				pr_debug("... -> %pap x 0x%x\n",
					 &cur->start, cur->size);
				kfree(e24_allocation);
			} else {
				pr_debug("inserting new free block\n");
				e24_allocation->next = cur;
				*pcur = e24_allocation;
			}
			break;
		}
	}

	mutex_unlock(&pool->free_list_lock);
}

static long e24_private_alloc(struct e24_allocation_pool *pool,
			      u32 size, u32 align,
			      struct e24_allocation **alloc)
{
	struct e24_private_pool *ppool = container_of(pool,
						      struct e24_private_pool,
						      pool);
	struct e24_allocation **pcur;
	struct e24_allocation *cur = NULL;
	struct e24_allocation *new;
	phys_addr_t aligned_start = 0;
	bool found = false;

	if (!size || (align & (align - 1)))
		return -EINVAL;
	if (!align)
		align = 1;

	new = kzalloc(sizeof(struct e24_allocation), GFP_KERNEL);
	if (!new)
		return -ENOMEM;

	align = ALIGN(align, PAGE_SIZE);
	size = ALIGN(size, PAGE_SIZE);

	mutex_lock(&ppool->free_list_lock);

	/* on exit free list is fixed */
	for (pcur = &ppool->free_list; *pcur; pcur = &(*pcur)->next) {
		cur = *pcur;
		aligned_start = ALIGN(cur->start, align);

		if (aligned_start >= cur->start &&
		    aligned_start - cur->start + size <= cur->size) {
			if (aligned_start == cur->start) {
				if (aligned_start + size == cur->start + cur->size) {
					pr_debug("reusing complete block: %pap x %x\n",
						 &cur->start, cur->size);
					*pcur = cur->next;
				} else {
					pr_debug("cutting block head: %pap x %x ->\n",
						 &cur->start, cur->size);
					cur->size -= aligned_start + size - cur->start;
					cur->start = aligned_start + size;
					pr_debug("... -> %pap x %x\n",
						 &cur->start, cur->size);
					cur = NULL;
				}
			} else {
				if (aligned_start + size == cur->start + cur->size) {
					pr_debug("cutting block tail: %pap x %x ->\n",
						 &cur->start, cur->size);
					cur->size = aligned_start - cur->start;
					pr_debug("... -> %pap x %x\n",
						 &cur->start, cur->size);
					cur = NULL;
				} else {
					pr_debug("splitting block into two: %pap x %x ->\n",
						 &cur->start, cur->size);
					new->start = aligned_start + size;
					new->size = cur->start +
						cur->size - new->start;

					cur->size = aligned_start - cur->start;

					new->next = cur->next;
					cur->next = new;
					pr_debug("... -> %pap x %x + %pap x %x\n",
						 &cur->start, cur->size,
						 &new->start, new->size);

					cur = NULL;
					new = NULL;
				}
			}
			found = true;
			break;
		} else {
			cur = NULL;
		}
	}

	mutex_unlock(&ppool->free_list_lock);

	if (!found) {
		kfree(cur);
		kfree(new);
		return -ENOMEM;
	}

	if (!cur) {
		cur = new;
		new = NULL;
	}
	if (!cur) {
		cur = kzalloc(sizeof(struct e24_allocation), GFP_KERNEL);
		if (!cur)
			return -ENOMEM;
	}

	kfree(new);
	pr_debug("returning: %pap x %x\n", &aligned_start, size);
	cur->start = aligned_start;
	cur->size = size;
	cur->pool = pool;
	atomic_set(&cur->ref, 0);
	atomic_inc(&cur->ref);
	*alloc = cur;

	return 0;
}

static void e24_private_free_pool(struct e24_allocation_pool *pool)
{
	struct e24_private_pool *ppool = container_of(pool,
						      struct e24_private_pool,
						      pool);
	kfree(ppool->free_list);
	kfree(ppool);
}

static phys_addr_t e24_private_offset(const struct e24_allocation *allocation)
{
	struct e24_private_pool *ppool = container_of(allocation->pool,
						      struct e24_private_pool,
						      pool);
	return allocation->start - ppool->start;
}

static const struct e24_allocation_ops e24_private_pool_ops = {
	.alloc = e24_private_alloc,
	.free = e24_private_free,
	.free_pool = e24_private_free_pool,
	.offset = e24_private_offset,
};

long e24_init_private_pool(struct e24_allocation_pool **ppool,
			   phys_addr_t start, u32 size)
{
	struct e24_private_pool *pool = kmalloc(sizeof(*pool), GFP_KERNEL);
	struct e24_allocation *allocation = kmalloc(sizeof(*allocation),
						    GFP_KERNEL);

	if (!pool || !allocation) {
		kfree(pool);
		kfree(allocation);
		return -ENOMEM;
	}

	*allocation = (struct e24_allocation){
		.pool = &pool->pool,
		.start = start,
		.size = size,
	};
	*pool = (struct e24_private_pool){
		.pool = {
			.ops = &e24_private_pool_ops,
		},
		.start = start,
		.size = size,
		.free_list = allocation,
	};
	mutex_init(&pool->free_list_lock);
	*ppool = &pool->pool;
	return 0;
}
