/* SPDX-License-Identifier: GPL-2.0 */
#ifndef E24_ALLOC_H
#define E24_ALLOC_H

struct e24_allocation_pool;
struct e24_allocation;

struct e24_allocation_ops {
	long (*alloc)(struct e24_allocation_pool *allocation_pool,
		      u32 size, u32 align, struct e24_allocation **alloc);
	void (*free)(struct e24_allocation *allocation);
	void (*free_pool)(struct e24_allocation_pool *allocation_pool);
	phys_addr_t (*offset)(const struct e24_allocation *allocation);
};

struct e24_allocation_pool {
	const struct e24_allocation_ops *ops;
};

struct e24_allocation {
	struct e24_allocation_pool *pool;
	struct e24_allocation *next;
	phys_addr_t start;
	u32 size;
	atomic_t ref;
};

static inline void e24_free_pool(struct e24_allocation_pool *allocation_pool)
{
	allocation_pool->ops->free_pool(allocation_pool);
}

static inline void e24_free(struct e24_allocation *allocation)
{
	return allocation->pool->ops->free(allocation);
}

static inline long e24_allocate(struct e24_allocation_pool *allocation_pool,
				u32 size, u32 align,
				struct e24_allocation **alloc)
{
	return allocation_pool->ops->alloc(allocation_pool,
					   size, align, alloc);
}

static inline void e24_allocation_put(struct e24_allocation *e24_allocation)
{
	if (atomic_dec_and_test(&e24_allocation->ref))
		e24_allocation->pool->ops->free(e24_allocation);
}

static inline phys_addr_t e24_allocation_offset(const struct e24_allocation *allocation)
{
	return allocation->pool->ops->offset(allocation);
}

long e24_init_private_pool(struct e24_allocation_pool **ppool, phys_addr_t start, u32 size);

#endif
