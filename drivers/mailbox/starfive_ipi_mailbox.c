// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 StarFive Technology Co., Ltd.
 */

#include <asm/irq.h>
#include <asm/sbi.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/bitfield.h>
#include <linux/mailbox_controller.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/suspend.h>
#include <linux/slab.h>

#include "mailbox.h"

#define IPI_MB_CHANS		2
#define IPI_MB_DEV_PER_CHAN	8

#define TX_MBOX_OFFSET		0x400

#define TX_DONE_OFFSET		0x100

#define QUEUE_ID_OFFSET		16
#define QUEUE_TO_CHAN		4

/* Please not change TX & RX */
enum ipi_mb_chan_type {
	IPI_MB_TYPE_RX		= 0, /* Rx */
	IPI_MB_TYPE_TX		= 1, /* Txdone */
};

struct ipi_mb_con_priv {
	unsigned int		idx;
	enum ipi_mb_chan_type	type;
	struct mbox_chan	*chan;
	struct tasklet_struct	txdb_tasklet;
	int rtos_hart_id;
};

struct ipi_mb_priv {
	struct device		*dev;

	struct mbox_controller	mbox;
	struct mbox_chan	chans[IPI_MB_CHANS * 2];

	struct ipi_mb_con_priv  con_priv_tx[IPI_MB_CHANS];
	struct ipi_mb_con_priv  con_priv_rx[IPI_MB_CHANS];

	void *tx_mbase;
	void *rx_mbase;
	int mem_size;
	int dev_num_per_chan;
};

struct ipi_mb_priv *mb_priv;

static struct ipi_mb_priv *to_ipi_mb_priv(struct mbox_controller *mbox)
{
	return container_of(mbox, struct ipi_mb_priv, mbox);
}

static int ipi_mb_generic_tx(struct ipi_mb_priv *priv,
			     struct ipi_mb_con_priv *cp,
			     void *data)
{
	unsigned long *msg = data, *mb_base;
	unsigned long queue;

	switch (cp->type) {
	case IPI_MB_TYPE_TX:
		queue = *msg >> (QUEUE_ID_OFFSET + 1);
		if (FIELD_GET(BIT(QUEUE_ID_OFFSET), *msg)) {
			mb_base = mb_priv->tx_mbase;
			WARN_ON(mb_base[queue]);
			xchg(&mb_base[queue], *msg);
			sbi_send_ipi_amp(cp->rtos_hart_id, IPI_MB_TYPE_TX); /* revert it */
		} else {
			mb_base = mb_priv->tx_mbase + TX_DONE_OFFSET;
			WARN_ON(mb_base[queue]);
			xchg(&mb_base[queue], *msg);
			sbi_send_ipi_amp(cp->rtos_hart_id, IPI_MB_TYPE_RX);
			tasklet_schedule(&cp->txdb_tasklet);
		}
		break;
	default:
		dev_warn_ratelimited(priv->dev,
				     "Send data on wrong channel type: %d\n", cp->type);
		return -EINVAL;
	}

	return 0;
}

static struct mbox_chan *queue_to_channel(unsigned long msg, bool tx)
{
	int index;
	int offset = QUEUE_ID_OFFSET + QUEUE_TO_CHAN;

	index = (tx) ? (msg >> offset) + IPI_MB_CHANS : (msg >> offset);

	return &mb_priv->chans[index];
}

static void ipi_mb_isr(unsigned long msg_type)
{
	unsigned long *mb_base, msg;
	struct mbox_chan *chan;
	void *rx_done_base;
	u32 i;

	if (!msg_type)
		return;

	mb_base = mb_priv->rx_mbase;
	rx_done_base = mb_priv->rx_mbase + TX_DONE_OFFSET;
	if (msg_type & BIT(IPI_MB_TYPE_RX)) {
		for (i = 0; i < IPI_MB_CHANS * mb_priv->dev_num_per_chan; i++) {
			msg = xchg(&mb_base[i], 0);
			chan = queue_to_channel(msg, 0);
			if (msg)
				mbox_chan_received_data(chan, (void *)&msg);
		}
	}
	if (msg_type & BIT(IPI_MB_TYPE_TX)) {
		mb_base = rx_done_base;
		for (i = 0; i < IPI_MB_CHANS * mb_priv->dev_num_per_chan; i++)  {
			msg = xchg(&mb_base[i], 0);
			chan = queue_to_channel(msg, 1);
			if (msg) {
				mbox_chan_received_data(chan, (void *)&msg);
				mbox_chan_txdone(chan, 0);
			}
		}
	}
}

static int ipi_mb_send_data(struct mbox_chan *chan, void *data)
{
	struct ipi_mb_priv *priv = to_ipi_mb_priv(chan->mbox);
	struct ipi_mb_con_priv *cp = chan->con_priv;

	return ipi_mb_generic_tx(priv, cp, data);
}

static void ipi_mb_txdb_tasklet(unsigned long data)
{
	struct ipi_mb_con_priv *cp = (struct ipi_mb_con_priv *)data;

	mbox_chan_txdone(cp->chan, 0);
}

static const struct mbox_chan_ops ipi_mb_ops = {
	.send_data = ipi_mb_send_data,
};

static struct mbox_chan *ipi_mb_xlate(struct mbox_controller *mbox,
				      const struct of_phandle_args *sp)
{
	struct mbox_chan *p_chan;
	u32 type, idx;

	if (sp->args_count != 2) {
		dev_err(mbox->dev, "Invalid argument count %d\n", sp->args_count);
		return ERR_PTR(-EINVAL);
	}

	type = sp->args[0]; /* channel type */
	idx = sp->args[1]; /* index */

	if (idx >= (mbox->num_chans >> 1)) {
		dev_err(mbox->dev,
			"Not supported channel number: %d. (type: %d, idx: %d)\n",
			idx, type, idx);
		return ERR_PTR(-EINVAL);
	}

	if (type == IPI_MB_TYPE_TX)
		p_chan = &mbox->chans[idx + IPI_MB_CHANS];
	else
		p_chan = &mbox->chans[idx];

	return p_chan;
}

static void ipi_mb_init_generic(struct ipi_mb_priv *priv, int rtos_hart_id)
{
	unsigned int i;

	for (i = 0; i < IPI_MB_CHANS; i++) {
		struct ipi_mb_con_priv *cp = &priv->con_priv_tx[i];

		cp->idx = i;
		cp->type = IPI_MB_TYPE_TX;
		cp->chan = &priv->chans[i + IPI_MB_CHANS];
		cp->rtos_hart_id = rtos_hart_id;
		tasklet_init(&cp->txdb_tasklet, ipi_mb_txdb_tasklet,
			     (unsigned long)cp);
		cp->chan->con_priv = cp;
	}
	for (i = 0; i < IPI_MB_CHANS; i++) {
		struct ipi_mb_con_priv *cp = &priv->con_priv_rx[i];

		cp->idx = i;
		cp->type = IPI_MB_TYPE_RX;
		cp->chan = &priv->chans[i];
		cp->rtos_hart_id = rtos_hart_id;
		cp->chan->con_priv = cp;
	}

	priv->mbox.num_chans = IPI_MB_CHANS * 2;
	priv->mbox.of_xlate = ipi_mb_xlate;
	priv->dev_num_per_chan = IPI_MB_DEV_PER_CHAN;

}

static int ipi_mb_init_mem_region(struct ipi_mb_priv *priv, struct platform_device *pdev)
{
	phys_addr_t phy_addr;
	struct resource *r;

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	phy_addr = r->start;
	priv->mem_size = resource_size(r);
	priv->rx_mbase = devm_memremap(priv->dev, phy_addr,
				       priv->mem_size,
				       MEMREMAP_WB);

	if (IS_ERR(priv->rx_mbase)) {
		dev_err(priv->dev, "unable to map memory region: %llx %d\n",
			(u64)r->start, priv->mem_size);
		return -EBUSY;
	}

	priv->tx_mbase = priv->rx_mbase + TX_MBOX_OFFSET;

	memset(priv->rx_mbase, 0, priv->mem_size);

	return 0;
}

static int starfive_ipi_mb_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct ipi_mb_priv *priv;
	u32 rtos_hart_id;
	int ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	if (of_property_read_u32(np, "rtos-hart-id",
				 &rtos_hart_id))
		return -EINVAL;

	priv->dev = dev;

	priv->mbox.dev = dev;
	priv->mbox.ops = &ipi_mb_ops;
	priv->mbox.chans = priv->chans;
	priv->mbox.txdone_irq = true;
	ipi_mb_init_generic(priv, rtos_hart_id);

	platform_set_drvdata(pdev, priv);

	ret = ipi_mb_init_mem_region(priv, pdev);
	if (ret)
		return ret;

	register_ipi_mailbox_handler(ipi_mb_isr);
	mb_priv = priv;

	ret = devm_mbox_controller_register(priv->dev, &priv->mbox);

	return ret;
}

static const struct of_device_id ipi_amp_of_match[] = {
	{ .compatible = "starfive,ipi-amp-mailbox", .data = NULL },
	{},
};
MODULE_DEVICE_TABLE(of, amp_rpmsg_of_match);

static struct platform_driver starfive_ipi_mb_driver = {
	.probe = starfive_ipi_mb_probe,
	.driver = {
		.name = "starfive-ipi-mailbox",
		.of_match_table = ipi_amp_of_match,
	},
};
module_platform_driver(starfive_ipi_mb_driver);
MODULE_LICENSE("GPL");

