// SPDX-License-Identifier: GPL-2.0
/*
 * PLDA PCIe XpressRich host controller driver
 *
 * Copyright (C) 2023 Microchip Co. Ltd
 *		      StarFive Co. Ltd.
 *
 * Author: Daire McNamara <daire.mcnamara@microchip.com>
 * Author: Minda Chen <minda.chen@starfivetech.com>
 */

#include <linux/irqchip/chained_irq.h>
#include <linux/irqdomain.h>
#include <linux/msi.h>
#include <linux/of_address.h>
#include <linux/of_pci.h>
#include <linux/pci_regs.h>
#include <linux/pci-ecam.h>
#include <linux/platform_device.h>

#include "pcie-plda.h"

void plda_handle_msi(struct irq_desc *desc)
{
	struct plda_pcie_rp *port = irq_desc_get_handler_data(desc);
	struct irq_chip *chip = irq_desc_get_chip(desc);
	struct device *dev = port->dev;
	struct plda_msi *msi = &port->msi;
	void __iomem *bridge_base_addr = port->bridge_addr;
	unsigned long status;
	u32 bit;
	int ret;

	chained_irq_enter(chip, desc);

	status = readl_relaxed(bridge_base_addr + ISTATUS_LOCAL);
	if (status & PM_MSI_INT_MSI_MASK) {
		writel_relaxed(status & PM_MSI_INT_MSI_MASK, bridge_base_addr + ISTATUS_LOCAL);
		status = readl_relaxed(bridge_base_addr + ISTATUS_MSI);
		for_each_set_bit(bit, &status, msi->num_vectors) {
			ret = generic_handle_domain_irq(msi->dev_domain, bit);
			if (ret)
				dev_err_ratelimited(dev, "bad MSI IRQ %d\n",
						    bit);
		}
	}

	chained_irq_exit(chip, desc);
}

static void plda_msi_bottom_irq_ack(struct irq_data *data)
{
	struct plda_pcie_rp *port = irq_data_get_irq_chip_data(data);
	void __iomem *bridge_base_addr = port->bridge_addr;
	u32 bitpos = data->hwirq;

	writel_relaxed(BIT(bitpos), bridge_base_addr + ISTATUS_MSI);
}

static void plda_compose_msi_msg(struct irq_data *data, struct msi_msg *msg)
{
	struct plda_pcie_rp *port = irq_data_get_irq_chip_data(data);
	phys_addr_t addr = port->msi.vector_phy;

	msg->address_lo = lower_32_bits(addr);
	msg->address_hi = upper_32_bits(addr);
	msg->data = data->hwirq;

	dev_dbg(port->dev, "msi#%x address_hi %#x address_lo %#x\n",
		(int)data->hwirq, msg->address_hi, msg->address_lo);
}

static int plda_msi_set_affinity(struct irq_data *irq_data,
				 const struct cpumask *mask, bool force)
{
	return -EINVAL;
}

static struct irq_chip plda_msi_bottom_irq_chip = {
	.name = "PLDA MSI",
	.irq_ack = plda_msi_bottom_irq_ack,
	.irq_compose_msi_msg = plda_compose_msi_msg,
	.irq_set_affinity = plda_msi_set_affinity,
};

int plda_irq_msi_domain_alloc(struct irq_domain *domain, unsigned int virq,
				     unsigned int nr_irqs, void *args)
{
	struct plda_pcie_rp *port = domain->host_data;
	struct plda_msi *msi = &port->msi;
	unsigned long bit;

	mutex_lock(&msi->lock);
	bit = find_first_zero_bit(msi->used, msi->num_vectors);
	if (bit >= msi->num_vectors) {
		mutex_unlock(&msi->lock);
		return -ENOSPC;
	}

	set_bit(bit, msi->used);

	irq_domain_set_info(domain, virq, bit, &plda_msi_bottom_irq_chip,
			    domain->host_data, handle_edge_irq, NULL, NULL);

	mutex_unlock(&msi->lock);

	return 0;
}

static void plda_irq_msi_domain_free(struct irq_domain *domain, unsigned int virq,
				     unsigned int nr_irqs)
{
	struct irq_data *d = irq_domain_get_irq_data(domain, virq);
	struct plda_pcie_rp *port = irq_data_get_irq_chip_data(d);
	struct plda_msi *msi = &port->msi;

	mutex_lock(&msi->lock);

	if (test_bit(d->hwirq, msi->used))
		__clear_bit(d->hwirq, msi->used);
	else
		dev_err(port->dev, "trying to free unused MSI%lu\n", d->hwirq);

	mutex_unlock(&msi->lock);
}

static const struct irq_domain_ops msi_domain_ops = {
	.alloc	= plda_irq_msi_domain_alloc,
	.free	= plda_irq_msi_domain_free,
};

static struct irq_chip plda_msi_irq_chip = {
	.name = "PLDA PCIe MSI",
	.irq_ack = irq_chip_ack_parent,
	.irq_mask = pci_msi_mask_irq,
	.irq_unmask = pci_msi_unmask_irq,
};

static struct msi_domain_info plda_msi_domain_info = {
	.flags = (MSI_FLAG_USE_DEF_DOM_OPS | MSI_FLAG_USE_DEF_CHIP_OPS |
		  MSI_FLAG_PCI_MSIX),
	.chip = &plda_msi_irq_chip,
};

int plda_allocate_msi_domains(struct plda_pcie_rp *port)
{
	struct device *dev = port->dev;
	struct fwnode_handle *fwnode = of_node_to_fwnode(dev->of_node);
	struct plda_msi *msi = &port->msi;

	mutex_init(&port->msi.lock);

	msi->dev_domain = irq_domain_add_linear(NULL, msi->num_vectors,
						&msi_domain_ops, port);
	if (!msi->dev_domain) {
		dev_err(dev, "failed to create IRQ domain\n");
		return -ENOMEM;
	}

	msi->msi_domain = pci_msi_create_irq_domain(fwnode, &plda_msi_domain_info,
						    msi->dev_domain);
	if (!msi->msi_domain) {
		dev_err(dev, "failed to create MSI domain\n");
		irq_domain_remove(msi->dev_domain);
		return -ENOMEM;
	}

	return 0;
}

void plda_handle_intx(struct irq_desc *desc)
{
	struct plda_pcie_rp *port = irq_desc_get_handler_data(desc);
	struct irq_chip *chip = irq_desc_get_chip(desc);
	struct device *dev = port->dev;
	void __iomem *bridge_base_addr = port->bridge_addr;
	unsigned long status;
	u32 bit;
	int ret;

	chained_irq_enter(chip, desc);

	status = readl_relaxed(bridge_base_addr + ISTATUS_LOCAL);
	if (status & PM_MSI_INT_INTX_MASK) {
		status &= PM_MSI_INT_INTX_MASK;
		status >>= PM_MSI_INT_INTX_SHIFT;
		for_each_set_bit(bit, &status, PCI_NUM_INTX) {
			ret = generic_handle_domain_irq(port->intx_domain, bit);
			if (ret)
				dev_err_ratelimited(dev, "bad INTx IRQ %d\n",
						    bit);
		}
	}

	chained_irq_exit(chip, desc);
}

static void plda_ack_intx_irq(struct irq_data *data)
{
	struct plda_pcie_rp *port = irq_data_get_irq_chip_data(data);
	void __iomem *bridge_base_addr = port->bridge_addr;
	u32 mask = BIT(data->hwirq + PM_MSI_INT_INTX_SHIFT);

	writel_relaxed(mask, bridge_base_addr + ISTATUS_LOCAL);
}

static void plda_mask_intx_irq(struct irq_data *data)
{
	struct plda_pcie_rp *port = irq_data_get_irq_chip_data(data);
	void __iomem *bridge_base_addr = port->bridge_addr;
	unsigned long flags;
	u32 mask = BIT(data->hwirq + PM_MSI_INT_INTX_SHIFT);
	u32 val;

	raw_spin_lock_irqsave(&port->lock, flags);
	val = readl_relaxed(bridge_base_addr + IMASK_LOCAL);
	val &= ~mask;
	writel_relaxed(val, bridge_base_addr + IMASK_LOCAL);
	raw_spin_unlock_irqrestore(&port->lock, flags);
}

static void plda_unmask_intx_irq(struct irq_data *data)
{
	struct plda_pcie_rp *port = irq_data_get_irq_chip_data(data);
	void __iomem *bridge_base_addr = port->bridge_addr;
	unsigned long flags;
	u32 mask = BIT(data->hwirq + PM_MSI_INT_INTX_SHIFT);
	u32 val;

	raw_spin_lock_irqsave(&port->lock, flags);
	val = readl_relaxed(bridge_base_addr + IMASK_LOCAL);
	val |= mask;
	writel_relaxed(val, bridge_base_addr + IMASK_LOCAL);
	raw_spin_unlock_irqrestore(&port->lock, flags);
}

static struct irq_chip plda_intx_irq_chip = {
	.name = "PLDA PCIe INTx",
	.irq_ack = plda_ack_intx_irq,
	.irq_mask = plda_mask_intx_irq,
	.irq_unmask = plda_unmask_intx_irq,
};

int plda_pcie_intx_map(struct irq_domain *domain, unsigned int irq,
			      irq_hw_number_t hwirq)
{
	irq_set_chip_and_handler(irq, &plda_intx_irq_chip, handle_level_irq);
	irq_set_chip_data(irq, domain->host_data);

	return 0;
}

void plda_pcie_setup_window(void __iomem *bridge_base_addr, u32 index,
			    phys_addr_t axi_addr, phys_addr_t pci_addr,
			    size_t size)
{
	u32 atr_sz = ilog2(size) - 1;
	u32 val;

	if (index == 0)
		val = PCIE_CONFIG_INTERFACE;
	else
		val = PCIE_TX_RX_INTERFACE;

	writel(val, bridge_base_addr + (index * ATR_ENTRY_SIZE) +
	       ATR0_AXI4_SLV0_TRSL_PARAM);

	val = lower_32_bits(axi_addr) | (atr_sz << ATR_SIZE_SHIFT) |
			    ATR_IMPL_ENABLE;
	writel(val, bridge_base_addr + (index * ATR_ENTRY_SIZE) +
	       ATR0_AXI4_SLV0_SRCADDR_PARAM);

	val = upper_32_bits(axi_addr);
	writel(val, bridge_base_addr + (index * ATR_ENTRY_SIZE) +
	       ATR0_AXI4_SLV0_SRC_ADDR);

	val = lower_32_bits(pci_addr);
	writel(val, bridge_base_addr + (index * ATR_ENTRY_SIZE) +
	       ATR0_AXI4_SLV0_TRSL_ADDR_LSB);

	val = upper_32_bits(pci_addr);
	writel(val, bridge_base_addr + (index * ATR_ENTRY_SIZE) +
	       ATR0_AXI4_SLV0_TRSL_ADDR_UDW);

	val = readl(bridge_base_addr + ATR0_PCIE_WIN0_SRCADDR_PARAM);
	val |= (ATR0_PCIE_ATR_SIZE << ATR0_PCIE_ATR_SIZE_SHIFT);
	writel(val, bridge_base_addr + ATR0_PCIE_WIN0_SRCADDR_PARAM);
	writel(0, bridge_base_addr + ATR0_PCIE_WIN0_SRC_ADDR);
}
EXPORT_SYMBOL_GPL(plda_pcie_setup_window);

int plda_pcie_setup_iomems(struct pci_host_bridge *bridge,
			   struct plda_pcie_rp *port)
{
	void __iomem *bridge_base_addr = port->bridge_addr;
	struct resource_entry *entry;
	u64 pci_addr;
	u32 index = 1;

	resource_list_for_each_entry(entry, &bridge->windows) {
		if (resource_type(entry->res) == IORESOURCE_MEM) {
			pci_addr = entry->res->start - entry->offset;
			plda_pcie_setup_window(bridge_base_addr, index,
					       entry->res->start, pci_addr,
					       resource_size(entry->res));
			index++;
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(plda_pcie_setup_iomems);
