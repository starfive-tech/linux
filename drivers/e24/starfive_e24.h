/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __STARFIVE_E24_H__
#define __STARFIVE_E24_H__

#include <linux/types.h>
#include <linux/completion.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/irqreturn.h>
#include <linux/platform_device.h>

#define E24_IOCTL_MAGIC 'e'
#define E24_IOCTL_SEND			_IO(E24_IOCTL_MAGIC, 1)
#define E24_IOCTL_RECV			_IO(E24_IOCTL_MAGIC, 2)
#define E24_IOCTL_GET_CHANNEL		_IO(E24_IOCTL_MAGIC, 3)
#define E24_IOCTL_FREE_CHANNEL		_IO(E24_IOCTL_MAGIC, 4)
#define E24_IOCTL_ALLOC			_IO(E24_IOCTL_MAGIC, 5)
#define E24_IOCTL_FREE			_IO(E24_IOCTL_MAGIC, 6)

#define E24_DSP_CMD_INLINE_DATA_SIZE	16
#define E24_NO_TRANSLATION		((u32)~0ul)
#define E24_CMD_STRIDE			256

#define E24_MEM_MAP
enum e24_irq_mode {
	MAIL_IRQ_NONE,
	MAIL_IRQ_LEVEL,
	MAIL_IRQ_MAX
};

enum {
	E24_FLAG_READ = 0x1,
	E24_FLAG_WRITE = 0x2,
	E24_FLAG_READ_WRITE = 0x3,
};

enum {
	E24_QUEUE_FLAG_VALID = 0x4,
	E24_QUEUE_FLAG_PRIO = 0xff00,
	E24_QUEUE_FLAG_PRIO_SHIFT = 8,

	E24_QUEUE_VALID_FLAGS =
		E24_QUEUE_FLAG_VALID |
		E24_QUEUE_FLAG_PRIO,
};

enum {
	E24_CMD_FLAG_REQUEST_VALID = 0x00000001,
	E24_CMD_FLAG_RESPONSE_VALID = 0x00000002,
	E24_CMD_FLAG_REQUEST_NSID = 0x00000004,
	E24_CMD_FLAG_RESPONSE_DELIVERY_FAIL = 0x00000008,
};

struct e24_address_map_entry {
	phys_addr_t src_addr;
	u32 dst_addr;
	u32 size;
};

struct e24_address_map {
	unsigned int n;
	struct e24_address_map_entry *entry;
};

struct e24_alien_mapping {
	unsigned long vaddr;
	unsigned long size;
	phys_addr_t paddr;
	void *allocation;
	enum {
		ALIEN_GUP,
		ALIEN_PFN_MAP,
		ALIEN_COPY,
	} type;
};

struct e24_mapping {
	enum {
		E24_MAPPING_NONE,
		E24_MAPPING_NATIVE,
		E24_MAPPING_ALIEN,
		E24_MAPPING_KERNEL = 0x4,
	} type;
	union {
		struct {
			struct e24_allocation *m_allocation;
			unsigned long vaddr;
		} native;
		struct e24_alien_mapping alien_mapping;
	};
};

struct e24_ioctl_alloc {
	u32 size;
	u32 align;
	u64 addr;
};

struct e24_comm {
	struct mutex lock;
	void __iomem *comm;
	struct completion completion;
	u32 priority;
};

struct e24_device {
	struct device *dev;
	const char *firmware_name;
	const struct firmware *firmware;
	struct miscdevice miscdev;
	const struct e24_hw_ops *hw_ops;
	void *hw_arg;
	int irq_mode;

	u32 n_queues;
	struct completion completion;
	struct e24_address_map address_map;
	struct e24_comm *queue;
	void __iomem *comm;
	phys_addr_t comm_phys;
	phys_addr_t shared_mem;
	phys_addr_t shared_size;

	u32 mbox_data;
	int nodeid;
	spinlock_t busy_list_lock;

	struct mbox_chan	*tx_channel;
	struct mbox_chan	*rx_channel;
	void			*rx_buffer;
	void			*message;
	struct e24_allocation_pool *pool;
	struct e24_allocation *busy_list;
};

struct e24_hw_arg {
	struct e24_device *e24;
	phys_addr_t regs_phys;
	struct clk *clk_rtc;
	struct clk *clk_core;
	struct clk *clk_dbg;
	struct reset_control *rst_core;
	struct regmap *reg_syscon;
	enum e24_irq_mode irq_mode;
};

static inline int e24_compare_address(phys_addr_t addr,
				      const struct e24_address_map_entry *entry)
{
	if (addr < entry->src_addr)
		return -1;
	if (addr - entry->src_addr < entry->size)
		return 0;
	return 1;
}

irqreturn_t e24_irq_handler(int irq, struct e24_device *e24_hw);

#endif
