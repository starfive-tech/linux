// SPDX-License-Identifier: GPL-2.0
/*
 * SiFive composable cache controller Driver
 *
 * Copyright (C) 2018-2022 SiFive, Inc.
 *
 */

#define pr_fmt(fmt) "CCACHE: " fmt

#include <linux/align.h>
#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/device.h>
#include <linux/bitfield.h>
#include <asm/cacheflush.h>
#include <asm/cacheinfo.h>
#include <asm/page.h>
#include "sifive_pl2.h"
#include <soc/sifive/sifive_ccache.h>

#define SIFIVE_CCACHE_DIRECCFIX_LOW 0x100
#define SIFIVE_CCACHE_DIRECCFIX_HIGH 0x104
#define SIFIVE_CCACHE_DIRECCFIX_COUNT 0x108

#define SIFIVE_CCACHE_DIRECCFAIL_LOW 0x120
#define SIFIVE_CCACHE_DIRECCFAIL_HIGH 0x124
#define SIFIVE_CCACHE_DIRECCFAIL_COUNT 0x128

#define SIFIVE_CCACHE_DATECCFIX_LOW 0x140
#define SIFIVE_CCACHE_DATECCFIX_HIGH 0x144
#define SIFIVE_CCACHE_DATECCFIX_COUNT 0x148

#define SIFIVE_CCACHE_DATECCFAIL_LOW 0x160
#define SIFIVE_CCACHE_DATECCFAIL_HIGH 0x164
#define SIFIVE_CCACHE_DATECCFAIL_COUNT 0x168

#define SIFIVE_CCACHE_CONFIG 0x00
#define SIFIVE_CCACHE_CONFIG_BANK_MASK GENMASK_ULL(7, 0)
#define SIFIVE_CCACHE_CONFIG_WAYS_MASK GENMASK_ULL(15, 8)
#define SIFIVE_CCACHE_CONFIG_SETS_MASK GENMASK_ULL(23, 16)
#define SIFIVE_CCACHE_CONFIG_BLKS_MASK GENMASK_ULL(31, 24)

#define SIFIVE_CCACHE_FLUSH64 0x200
#define SIFIVE_CCACHE_FLUSH32 0x240

#define SIFIVE_L2_WAYMASK_BASE 0x800
#define SIFIVE_L2_MAX_MASTER_ID 32

#define SIFIVE_CCACHE_WAYENABLE 0x08
#define SIFIVE_CCACHE_ECCINJECTERR 0x40

#define SIFIVE_CCACHE_MAX_ECCINTR 4
#define SIFIVE_L2_DEFAULT_WAY_MASK 0xffff

static void __iomem *ccache_base;
static int g_irq[SIFIVE_CCACHE_MAX_ECCINTR];
static struct riscv_cacheinfo_ops ccache_cache_ops;
static int level;

static u32 flush_line_len;
static u32 cache_size;
static u32 cache_size_per_way;
static u32 cache_max_line;
static u32 cache_ways_per_bank;
static u32 cache_size_per_block;
static u32 cache_max_enabled_way;
static void __iomem *l2_zero_device_base;

enum {
	DIR_CORR = 0,
	DATA_CORR,
	DATA_UNCORR,
	DIR_UNCORR,
};

#ifdef CONFIG_DEBUG_FS
static struct dentry *sifive_test;

static ssize_t ccache_write(struct file *file, const char __user *data,
			    size_t count, loff_t *ppos)
{
	unsigned int val;

	if (kstrtouint_from_user(data, count, 0, &val))
		return -EINVAL;
	if ((val < 0xFF) || (val >= 0x10000 && val < 0x100FF))
		writel(val, ccache_base + SIFIVE_CCACHE_ECCINJECTERR);
	else
		return -EINVAL;
	return count;
}

static const struct file_operations ccache_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.write = ccache_write
};

static void setup_sifive_debug(void)
{
	sifive_test = debugfs_create_dir("sifive_ccache_cache", NULL);

	debugfs_create_file("sifive_debug_inject_error", 0200,
			    sifive_test, NULL, &ccache_fops);
}
#endif

static void ccache_config_read(void)
{
	u32 cfg;

	cfg = readl(ccache_base + SIFIVE_CCACHE_CONFIG);
	cache_ways_per_bank = FIELD_GET(SIFIVE_CCACHE_CONFIG_WAYS_MASK, cfg);
	cache_size_per_block = BIT_ULL(FIELD_GET(SIFIVE_CCACHE_CONFIG_BLKS_MASK, cfg));
	flush_line_len = cache_size_per_block;
	cache_size_per_way = cache_size / cache_ways_per_bank;
	cache_max_line = cache_size_per_way / flush_line_len - 1;

	pr_info("%llu banks, %u ways, sets/bank=%llu, bytes/block=%u\n",
		FIELD_GET(SIFIVE_CCACHE_CONFIG_BANK_MASK, cfg),
		cache_ways_per_bank,
		BIT_ULL(FIELD_GET(SIFIVE_CCACHE_CONFIG_SETS_MASK, cfg)),
		cache_size_per_block);

	pr_info("max_line=%u, %u bytes/line, %u bytes/way\n",
		cache_max_line,
		flush_line_len,
		cache_size_per_way);

	cache_max_enabled_way = readl(ccache_base + SIFIVE_CCACHE_WAYENABLE);
	pr_info("Index of the largest way enabled: %u\n", cache_max_enabled_way);
}

static const struct of_device_id sifive_ccache_ids[] = {
	{ .compatible = "sifive,fu540-c000-ccache" },
	{ .compatible = "sifive,fu740-c000-ccache" },
	{ .compatible = "sifive,ccache0" },
	{ /* end of table */ }
};

static ATOMIC_NOTIFIER_HEAD(ccache_err_chain);

int register_sifive_ccache_error_notifier(struct notifier_block *nb)
{
	return atomic_notifier_chain_register(&ccache_err_chain, nb);
}
EXPORT_SYMBOL_GPL(register_sifive_ccache_error_notifier);

int unregister_sifive_ccache_error_notifier(struct notifier_block *nb)
{
	return atomic_notifier_chain_unregister(&ccache_err_chain, nb);
}
EXPORT_SYMBOL_GPL(unregister_sifive_ccache_error_notifier);

#ifdef CONFIG_RISCV_DMA_NONCOHERENT
static phys_addr_t uncached_offset;
DEFINE_STATIC_KEY_FALSE(sifive_ccache_handle_noncoherent_key);

static void sifive_ccache_flush_range_by_way_index(u32 start_index, u32 end_index)
{
	u32 way, master;
	u32 index;
	u64 way_mask;
	void __iomem *line_addr, *addr;

	mb();
	way_mask = 1;
	line_addr = l2_zero_device_base + start_index * flush_line_len;
	for (way = 0; way <= cache_max_enabled_way; ++way) {
		addr = ccache_base + SIFIVE_L2_WAYMASK_BASE;
		for (master = 0; master < SIFIVE_L2_MAX_MASTER_ID; ++master) {
			writeq_relaxed(way_mask, addr);
			addr += 8;
		}

		addr = line_addr;
		for (index = start_index; index <= end_index; ++index) {
#ifdef CONFIG_32BIT
			writel_relaxed(0, addr);
#else
			writeq_relaxed(0, addr);
#endif
			addr += flush_line_len;
		}

		way_mask <<= 1;
		line_addr += cache_size_per_way;
		mb();
	}

	addr = ccache_base + SIFIVE_L2_WAYMASK_BASE;
	for (master = 0; master < SIFIVE_L2_MAX_MASTER_ID; ++master) {
		writeq_relaxed(SIFIVE_L2_DEFAULT_WAY_MASK, addr);
		addr += 8;
	}
	mb();
}

void sifive_ccache_flush_entire(void)
{
	sifive_ccache_flush_range_by_way_index(0, cache_max_line);
}
EXPORT_SYMBOL_GPL(sifive_ccache_flush_entire);

void sifive_ccache_flush_range(phys_addr_t start, size_t len)
{
	phys_addr_t end = start + len;
	phys_addr_t line;

	if (!len)
		return;

	mb();
	for (line = ALIGN_DOWN(start, flush_line_len); line < end;
	     line += flush_line_len) {
#ifdef CONFIG_32BIT
		writel(line >> 4, ccache_base + SIFIVE_CCACHE_FLUSH32);
#else
		writeq(line, ccache_base + SIFIVE_CCACHE_FLUSH64);
#endif
		mb();
	}
}
EXPORT_SYMBOL_GPL(sifive_ccache_flush_range);

void sifive_l2_flush64_range(phys_addr_t start, size_t len)
{
	sifive_ccache_flush_range(start, len);
}
EXPORT_SYMBOL_GPL(sifive_l2_flush64_range);

void *sifive_ccache_set_uncached(void *addr, size_t size)
{
	phys_addr_t phys_addr = __pa(addr) + uncached_offset;
	void *mem_base;

	mem_base = memremap(phys_addr, size, MEMREMAP_WT);
	if (!mem_base) {
		pr_err("%s memremap failed for addr %p\n", __func__, addr);
		return ERR_PTR(-EINVAL);
	}

	return mem_base;
}
EXPORT_SYMBOL_GPL(sifive_ccache_set_uncached);
#endif /* CONFIG_RISCV_DMA_NONCOHERENT */

static int ccache_largest_wayenabled(void)
{
	return readl(ccache_base + SIFIVE_CCACHE_WAYENABLE) & 0xFF;
}

static ssize_t number_of_ways_enabled_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	return sprintf(buf, "%u\n", ccache_largest_wayenabled());
}

static DEVICE_ATTR_RO(number_of_ways_enabled);

static struct attribute *priv_attrs[] = {
	&dev_attr_number_of_ways_enabled.attr,
	NULL,
};

static const struct attribute_group priv_attr_group = {
	.attrs = priv_attrs,
};

static const struct attribute_group *ccache_get_priv_group(struct cacheinfo
							   *this_leaf)
{
	/* We want to use private group for composable cache only */
	if (this_leaf->level == level)
		return &priv_attr_group;
	else
		return NULL;
}

static irqreturn_t ccache_int_handler(int irq, void *device)
{
	unsigned int add_h, add_l;

	if (irq == g_irq[DIR_CORR]) {
		add_h = readl(ccache_base + SIFIVE_CCACHE_DIRECCFIX_HIGH);
		add_l = readl(ccache_base + SIFIVE_CCACHE_DIRECCFIX_LOW);
		pr_err("DirError @ 0x%08X.%08X\n", add_h, add_l);
		/* Reading this register clears the DirError interrupt sig */
		readl(ccache_base + SIFIVE_CCACHE_DIRECCFIX_COUNT);
		atomic_notifier_call_chain(&ccache_err_chain,
					   SIFIVE_CCACHE_ERR_TYPE_CE,
					   "DirECCFix");
	}
	if (irq == g_irq[DIR_UNCORR]) {
		add_h = readl(ccache_base + SIFIVE_CCACHE_DIRECCFAIL_HIGH);
		add_l = readl(ccache_base + SIFIVE_CCACHE_DIRECCFAIL_LOW);
		/* Reading this register clears the DirFail interrupt sig */
		readl(ccache_base + SIFIVE_CCACHE_DIRECCFAIL_COUNT);
		atomic_notifier_call_chain(&ccache_err_chain,
					   SIFIVE_CCACHE_ERR_TYPE_UE,
					   "DirECCFail");
		panic("CCACHE: DirFail @ 0x%08X.%08X\n", add_h, add_l);
	}
	if (irq == g_irq[DATA_CORR]) {
		add_h = readl(ccache_base + SIFIVE_CCACHE_DATECCFIX_HIGH);
		add_l = readl(ccache_base + SIFIVE_CCACHE_DATECCFIX_LOW);
		pr_err("DataError @ 0x%08X.%08X\n", add_h, add_l);
		/* Reading this register clears the DataError interrupt sig */
		readl(ccache_base + SIFIVE_CCACHE_DATECCFIX_COUNT);
		atomic_notifier_call_chain(&ccache_err_chain,
					   SIFIVE_CCACHE_ERR_TYPE_CE,
					   "DatECCFix");
	}
	if (irq == g_irq[DATA_UNCORR]) {
		add_h = readl(ccache_base + SIFIVE_CCACHE_DATECCFAIL_HIGH);
		add_l = readl(ccache_base + SIFIVE_CCACHE_DATECCFAIL_LOW);
		pr_err("DataFail @ 0x%08X.%08X\n", add_h, add_l);
		/* Reading this register clears the DataFail interrupt sig */
		readl(ccache_base + SIFIVE_CCACHE_DATECCFAIL_COUNT);
		atomic_notifier_call_chain(&ccache_err_chain,
					   SIFIVE_CCACHE_ERR_TYPE_UE,
					   "DatECCFail");
	}

	return IRQ_HANDLED;
}

static int __init sifive_ccache_init(void)
{
	struct device_node *np;
	struct resource res;
	int i, rc, intr_num, cpu;
	u64 __maybe_unused offset;

	np = of_find_matching_node(NULL, sifive_ccache_ids);
	if (!np)
		return -ENODEV;

	if (of_address_to_resource(np, 0, &res)) {
		rc = -ENODEV;
		goto err_node_put;
	}

	ccache_base = ioremap(res.start, resource_size(&res));
	if (!ccache_base) {
		rc = -ENOMEM;
		goto err_node_put;
	}

	if (of_address_to_resource(np, 2, &res))
		return -ENODEV;

	l2_zero_device_base = ioremap(res.start, resource_size(&res));
	if (!l2_zero_device_base)
		return -ENOMEM;

	if (of_property_read_u32(np, "cache-size", &cache_size)) {
		pr_err("L2CACHE: no cache-size property\n");
		return -ENODEV;
	}

	if (of_property_read_u32(np, "cache-level", &level)) {
		rc = -ENOENT;
		goto err_unmap;
	}

	intr_num = of_property_count_u32_elems(np, "interrupts");
	if (!intr_num) {
		pr_err("No interrupts property\n");
		rc = -ENODEV;
		goto err_unmap;
	}

	for (i = 0; i < intr_num; i++) {
		g_irq[i] = irq_of_parse_and_map(np, i);
		rc = request_irq(g_irq[i], ccache_int_handler, 0, "ccache_ecc",
				 NULL);
		if (rc) {
			pr_err("Could not request IRQ %d\n", g_irq[i]);
			goto err_free_irq;
		}
	}
	of_node_put(np);

	ccache_config_read();

#ifdef CONFIG_RISCV_DMA_NONCOHERENT
	if (!of_property_read_u64(np, "uncached-offset", &offset)) {
		uncached_offset = offset;
		static_branch_enable(&sifive_ccache_handle_noncoherent_key);
		riscv_cbom_block_size = flush_line_len;
		riscv_noncoherent_supported();
	}
#endif

	if (IS_ENABLED(CONFIG_SIFIVE_U74_L2_PMU)) {
		for_each_cpu(cpu, cpu_possible_mask) {
			rc = sifive_u74_l2_pmu_probe(np, ccache_base, cpu);
			if (rc) {
				pr_err("Failed to probe sifive_u74_l2_pmu driver.\n");
				return -EINVAL;
			}
		}
	}

	ccache_cache_ops.get_priv_group = ccache_get_priv_group;
	riscv_set_cacheinfo_ops(&ccache_cache_ops);

#ifdef CONFIG_DEBUG_FS
	setup_sifive_debug();
#endif
	return 0;

err_free_irq:
	while (--i >= 0)
		free_irq(g_irq[i], NULL);
err_unmap:
	iounmap(ccache_base);
err_node_put:
	of_node_put(np);
	return rc;
}

arch_initcall(sifive_ccache_init);
