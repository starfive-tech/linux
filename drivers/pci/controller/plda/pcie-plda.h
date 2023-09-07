/* SPDX-License-Identifier: GPL-2.0 */
/*
 * PLDA PCIe host controller driver
 */

#ifndef _PCIE_PLDA_H
#define _PCIE_PLDA_H

#include <linux/phy/phy.h>

/* Number of MSI IRQs */
#define PLDA_MAX_NUM_MSI_IRQS			32

/* PCIe Bridge Phy Regs */
#define GEN_SETTINGS				0x80
#define  RP_ENABLE				1
#define PCIE_PCI_IDS_DW1			0x9c
#define  IDS_CLASS_CODE_SHIFT			16
#define PCIE_PCI_IRQ_DW0			0xa8
#define  MSIX_CAP_MASK				BIT(31)
#define  NUM_MSI_MSGS_MASK			GENMASK(6, 4)
#define  NUM_MSI_MSGS_SHIFT			4
#define PCI_MISC				0xb4
#define  PHY_FUNCTION_DIS			BIT(15)
#define PCIE_WINROM				0xfc
#define  PREF_MEM_WIN_64_SUPPORT		BIT(3)

#define IMASK_LOCAL				0x180
#define  DMA_END_ENGINE_0_MASK			0x00000000u
#define  DMA_END_ENGINE_0_SHIFT			0
#define  DMA_END_ENGINE_1_MASK			0x00000000u
#define  DMA_END_ENGINE_1_SHIFT			1
#define  DMA_ERROR_ENGINE_0_MASK		0x00000100u
#define  DMA_ERROR_ENGINE_0_SHIFT		8
#define  DMA_ERROR_ENGINE_1_MASK		0x00000200u
#define  DMA_ERROR_ENGINE_1_SHIFT		9
#define  A_ATR_EVT_POST_ERR_MASK		0x00010000u
#define  A_ATR_EVT_POST_ERR_SHIFT		16
#define  A_ATR_EVT_FETCH_ERR_MASK		0x00020000u
#define  A_ATR_EVT_FETCH_ERR_SHIFT		17
#define  A_ATR_EVT_DISCARD_ERR_MASK		0x00040000u
#define  A_ATR_EVT_DISCARD_ERR_SHIFT		18
#define  A_ATR_EVT_DOORBELL_MASK		0x00000000u
#define  A_ATR_EVT_DOORBELL_SHIFT		19
#define  P_ATR_EVT_POST_ERR_MASK		0x00100000u
#define  P_ATR_EVT_POST_ERR_SHIFT		20
#define  P_ATR_EVT_FETCH_ERR_MASK		0x00200000u
#define  P_ATR_EVT_FETCH_ERR_SHIFT		21
#define  P_ATR_EVT_DISCARD_ERR_MASK		0x00400000u
#define  P_ATR_EVT_DISCARD_ERR_SHIFT		22
#define  P_ATR_EVT_DOORBELL_MASK		0x00000000u
#define  P_ATR_EVT_DOORBELL_SHIFT		23
#define  PM_MSI_INT_INTA_MASK			0x01000000u
#define  PM_MSI_INT_INTA_SHIFT			24
#define  PM_MSI_INT_INTB_MASK			0x02000000u
#define  PM_MSI_INT_INTB_SHIFT			25
#define  PM_MSI_INT_INTC_MASK			0x04000000u
#define  PM_MSI_INT_INTC_SHIFT			26
#define  PM_MSI_INT_INTD_MASK			0x08000000u
#define  PM_MSI_INT_INTD_SHIFT			27
#define  PM_MSI_INT_INTX_MASK			0x0f000000u
#define  PM_MSI_INT_INTX_SHIFT			24
#define  PM_MSI_INT_MSI_MASK			0x10000000u
#define  PM_MSI_INT_MSI_SHIFT			28
#define  PM_MSI_INT_AER_EVT_MASK		0x20000000u
#define  PM_MSI_INT_AER_EVT_SHIFT		29
#define  PM_MSI_INT_EVENTS_MASK			0x40000000u
#define  PM_MSI_INT_EVENTS_SHIFT		30
#define  PM_MSI_INT_SYS_ERR_MASK		0x80000000u
#define  PM_MSI_INT_SYS_ERR_SHIFT		31
#define  NUM_LOCAL_EVENTS			15
#define ISTATUS_LOCAL				0x184
#define IMASK_HOST				0x188
#define ISTATUS_HOST				0x18c
#define IMSI_ADDR				0x190
#define ISTATUS_MSI				0x194
#define PMSG_SUPPORT_RX				0x3f0
#define  PMSG_LTR_SUPPORT			BIT(2)

/* PCIe Master table init defines */
#define ATR0_PCIE_WIN0_SRCADDR_PARAM		0x600u
#define  ATR0_PCIE_ATR_SIZE			0x25
#define  ATR0_PCIE_ATR_SIZE_SHIFT		1
#define ATR0_PCIE_WIN0_SRC_ADDR			0x604u
#define ATR0_PCIE_WIN0_TRSL_ADDR_LSB		0x608u
#define ATR0_PCIE_WIN0_TRSL_ADDR_UDW		0x60cu
#define ATR0_PCIE_WIN0_TRSL_PARAM		0x610u

/* PCIe AXI slave table init defines */
#define ATR0_AXI4_SLV0_SRCADDR_PARAM		0x800u
#define  ATR_SIZE_SHIFT				1
#define  ATR_IMPL_ENABLE			1
#define ATR0_AXI4_SLV0_SRC_ADDR			0x804u
#define ATR0_AXI4_SLV0_TRSL_ADDR_LSB		0x808u
#define ATR0_AXI4_SLV0_TRSL_ADDR_UDW		0x80cu
#define ATR0_AXI4_SLV0_TRSL_PARAM		0x810u
#define  PCIE_TX_RX_INTERFACE			0x00000000u
#define  PCIE_CONFIG_INTERFACE			0x00000001u

#define CONFIG_SPACE_ADDR			0x1000u

#define ATR_ENTRY_SIZE				32

#define EVENT_A_ATR_EVT_POST_ERR		0
#define EVENT_A_ATR_EVT_FETCH_ERR		1
#define EVENT_A_ATR_EVT_DISCARD_ERR		2
#define EVENT_A_ATR_EVT_DOORBELL		3
#define EVENT_P_ATR_EVT_POST_ERR		4
#define EVENT_P_ATR_EVT_FETCH_ERR		5
#define EVENT_P_ATR_EVT_DISCARD_ERR		6
#define EVENT_P_ATR_EVT_DOORBELL		7
#define EVENT_PM_MSI_INT_INTX			8
#define EVENT_PM_MSI_INT_MSI			9
#define EVENT_PM_MSI_INT_AER_EVT		10
#define EVENT_PM_MSI_INT_EVENTS			11
#define EVENT_PM_MSI_INT_SYS_ERR		12
#define NUM_PLDA_EVENTS				13

#define PM_MSI_TO_MASK_OFFSET			19

struct plda_pcie_rp;

struct plda_event_ops {
	u32 (*get_events)(struct plda_pcie_rp *pcie);
};

struct plda_pcie_host_ops {
	int (*host_init)(struct plda_pcie_rp *pcie);
	void (*host_deinit)(struct plda_pcie_rp *pcie);
};

struct plda_msi {
	struct mutex lock;		/* Protect used bitmap */
	struct irq_domain *msi_domain;
	struct irq_domain *dev_domain;
	u32 num_vectors;
	u64 vector_phy;
	DECLARE_BITMAP(used, PLDA_MAX_NUM_MSI_IRQS);
};

struct plda_pcie_rp {
	struct device *dev;
	struct pci_host_bridge *bridge;
	struct irq_domain *intx_domain;
	struct irq_domain *event_domain;
	raw_spinlock_t lock;
	struct plda_msi msi;
	const struct plda_event_ops *event_ops;
	const struct plda_pcie_host_ops *host_ops;
	struct phy *phy;
	void __iomem *bridge_addr;
	void __iomem *config_base;
	int irq;
	int msi_irq;
	int intx_irq;
	int num_events;
};

struct plda_event {
	const struct irq_domain_ops *domain_ops;
	const struct plda_event_ops *event_ops;
	int (*request_event_irq)(struct plda_pcie_rp *pcie,
				 int event_irq, int event);
	int intx_event;
	int msi_event;
};

void __iomem *plda_pcie_map_bus(struct pci_bus *bus, unsigned int devfn, int where);
int plda_init_interrupts(struct platform_device *pdev,
			 struct plda_pcie_rp *port,
			 const struct plda_event *event);
void plda_pcie_setup_window(void __iomem *bridge_base_addr, u32 index,
			    phys_addr_t axi_addr, phys_addr_t pci_addr,
			    size_t size);
int plda_pcie_setup_iomems(struct pci_host_bridge *bridge,
			   struct plda_pcie_rp *port);
int plda_pcie_host_init(struct plda_pcie_rp *pcie, struct pci_ops *ops);
void plda_pcie_host_deinit(struct plda_pcie_rp *pcie);

static inline void plda_set_default_msi(struct plda_msi *msi)
{
	msi->vector_phy = IMSI_ADDR;
	msi->num_vectors = PLDA_MAX_NUM_MSI_IRQS;
}

static inline void plda_pcie_enable_root_port(struct plda_pcie_rp *plda)
{
	u32 value;

	value = readl_relaxed(plda->bridge_addr + GEN_SETTINGS);
	value |= RP_ENABLE;
	writel_relaxed(value, plda->bridge_addr + GEN_SETTINGS);
}

static inline void plda_pcie_set_standard_class(struct plda_pcie_rp *plda)
{
	u32 value;

	value = readl_relaxed(plda->bridge_addr + PCIE_PCI_IDS_DW1);
	value &= 0xff;
	value |= (PCI_CLASS_BRIDGE_PCI << IDS_CLASS_CODE_SHIFT);
	writel_relaxed(value, plda->bridge_addr + PCIE_PCI_IDS_DW1);
}

static inline void plda_pcie_set_pref_win_64bit(struct plda_pcie_rp *plda)
{
	u32 value;

	value = readl_relaxed(plda->bridge_addr + PCIE_WINROM);
	value |= PREF_MEM_WIN_64_SUPPORT;
	writel_relaxed(value, plda->bridge_addr + PCIE_WINROM);
}

static inline void plda_pcie_disable_ltr(struct plda_pcie_rp *plda)
{
	u32 value;

	value = readl_relaxed(plda->bridge_addr + PMSG_SUPPORT_RX);
	value &= ~PMSG_LTR_SUPPORT;
	writel_relaxed(value, plda->bridge_addr + PMSG_SUPPORT_RX);
}

static inline void plda_pcie_disable_func(struct plda_pcie_rp *plda)
{
	u32 value;

	value = readl_relaxed(plda->bridge_addr + PCI_MISC);
	value |= PHY_FUNCTION_DIS;
	writel_relaxed(value, plda->bridge_addr + PCI_MISC);
}

static inline void plda_pcie_write_rc_bar(struct plda_pcie_rp *plda, u64 val)
{
	void __iomem *addr = plda->bridge_addr + CONFIG_SPACE_ADDR;

	writel_relaxed(val & 0xffffffff, addr + PCI_BASE_ADDRESS_0);
	writel_relaxed(val >> 32, addr + PCI_BASE_ADDRESS_1);
}
#endif /* _PCIE_PLDA_H */
