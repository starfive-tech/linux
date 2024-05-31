// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 StarFive Technology Co., Ltd.
 */

#include <asm/irq.h>
#include <linux/circ_buf.h>
#include <linux/dma-map-ops.h>
#include <linux/export.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/mailbox_client.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/remoteproc.h>
#include <linux/platform_device.h>
#include <linux/virtio.h>
#include <linux/virtio_config.h>
#include <linux/virtio_ids.h>
#include <linux/virtio_ring.h>
#include <linux/err.h>
#include <linux/kref.h>
#include <linux/slab.h>

struct starfive_virdev {
	struct virtio_device vdev;
	void *vring[2];
	struct virtqueue *vq[2];
	int base_vq_id;
	int num_of_vqs;
	int index;
	int status;
	struct starfive_rpmsg_dev *rpdev;
};

struct starfive_rpmsg_mem {
	void __iomem *vaddr;
	phys_addr_t phy_addr;
	unsigned long virtio_ram;
	int size;
	int count;
};

struct starfive_rpmsg_dev;

struct starfive_vq_priv {
	struct starfive_rpmsg_dev *rpdev;
	unsigned long mmsg;
	int vq_id;
};

struct starfive_rpmsg_dev {
	struct mbox_client cl;
	struct mbox_chan *tx_ch;
	struct mbox_chan *rx_ch;
	int vdev_nums;
	/* int first_notify; */
	u32 flags;
#define MAX_VDEV_NUMS  8
	struct starfive_virdev ivdev[MAX_VDEV_NUMS];
	struct starfive_rpmsg_mem shm_mem;
	struct starfive_vq_priv vq_priv[MAX_VDEV_NUMS * 2];
	struct delayed_work rpmsg_work;
	struct circ_buf rx_buffer;
	spinlock_t mu_lock;
	struct platform_device *pdev;
};

#define VRING_BUFFER_SIZE 8192
#define CIRC_ADD(val, add, size) (((val) + (add)) & ((size) - 1))
#define RPMSG_NUM_BUFS		(512)
#define MAX_RPMSG_BUF_SIZE	(512)

static inline struct starfive_virdev *vdev_to_virdev(struct virtio_device *dev)
{
	return container_of(dev, struct starfive_virdev, vdev);
}

static void rpmsg_work_handler(struct work_struct *work)
{
	u32 message;
	unsigned long flags;
	struct starfive_virdev *virdev;
	struct delayed_work *dwork = to_delayed_work(work);
	struct starfive_rpmsg_dev *rpdev = container_of(dwork,
			struct starfive_rpmsg_dev, rpmsg_work);
	struct circ_buf *cb = &rpdev->rx_buffer;
	struct platform_device *pdev = rpdev->pdev;
	struct device *dev = &pdev->dev;

	spin_lock_irqsave(&rpdev->mu_lock, flags);
	/* handle all incoming mu message */
	while (CIRC_CNT(cb->head, cb->tail, PAGE_SIZE)) {
		message = cb->buf[cb->tail];
		message |= (cb->buf[cb->tail + 1] << 8);
		message |= (cb->buf[cb->tail + 2] << 16);
		message |= (cb->buf[cb->tail + 3] << 24);
		spin_unlock_irqrestore(&rpdev->mu_lock, flags);
		virdev = &rpdev->ivdev[(message >> 16) / 2];

		dev_dbg(dev, "%s msg: 0x%x\n", __func__, message);

		message = message >> 16;
		message -= virdev->base_vq_id;
		/*
		 * Currently both PENDING_MSG and explicit-virtqueue-index
		 * messaging are supported.
		 * Whatever approach is taken, at this point message contains
		 * the index of the vring which was just triggered.
		 */
		if (message  < virdev->num_of_vqs)
			vring_interrupt(message, virdev->vq[message]);
		spin_lock_irqsave(&rpdev->mu_lock, flags);
		cb->tail = CIRC_ADD(cb->tail, 4, PAGE_SIZE);
	}
	spin_unlock_irqrestore(&rpdev->mu_lock, flags);
}

static void starfive_rpmsg_rx_callback(struct mbox_client *c, void *msg)
{
	int buf_space;
	u32 *data = msg;
	struct starfive_rpmsg_dev *rpdev = container_of(c,
			struct starfive_rpmsg_dev, cl);
	struct circ_buf *cb = &rpdev->rx_buffer;

	spin_lock(&rpdev->mu_lock);
	buf_space = CIRC_SPACE(cb->head, cb->tail, PAGE_SIZE);
	if (unlikely(!buf_space)) {
		dev_err(c->dev, "RPMSG RX overflow!\n");
		spin_unlock(&rpdev->mu_lock);
		return;
	}
	cb->buf[cb->head] = (u8) *data;
	cb->buf[cb->head + 1] = (u8) (*data >> 8);
	cb->buf[cb->head + 2] = (u8) (*data >> 16);
	cb->buf[cb->head + 3] = (u8) (*data >> 24);
	cb->head = CIRC_ADD(cb->head, 4, PAGE_SIZE);
	spin_unlock(&rpdev->mu_lock);

	schedule_delayed_work(&(rpdev->rpmsg_work), 0);
}

static unsigned long starfive_vring_init(struct vring *vr,
					 unsigned int num, void *p,
					 unsigned long align)
{
	unsigned long addr;

	vr->num   = num;
	vr->desc  = (struct vring_desc *)(void *)p;

	addr = (unsigned long) (p + num * sizeof(struct vring_desc) + align - 1UL);
	addr &= ~(align - 1UL);
	vr->avail = (void *)addr;
	addr = (u64)&vr->avail->ring[num];
	addr += (align - 1UL);
	addr &= ~(align - 1UL);
	vr->used  = (struct vring_used *)addr;
	addr = (unsigned long)&vr->used->ring[num];
	addr += (align - 1UL);
	addr &= ~(align - 1UL);

	return addr;
}

static bool starfive_virtio_notify(struct virtqueue *vq)
{
	int ret;
	struct starfive_vq_priv *rpvq = vq->priv;
	struct starfive_rpmsg_dev *rpdev = rpvq->rpdev;

	rpvq->mmsg = rpvq->vq_id << 16 | 0x1;

	rpdev->cl.tx_tout = 1000;
	ret = mbox_send_message(rpdev->tx_ch, &rpvq->mmsg);
	if (ret < 0)
		return false;

	return true;
}

static struct virtqueue *rp_find_vq(struct virtio_device *vdev,
				    unsigned int id,
				    void (*callback)(struct virtqueue *vq),
				    const char *name, bool ctx)
{
	struct starfive_virdev *virdev = vdev_to_virdev(vdev);
	struct starfive_rpmsg_dev *rpdev = virdev->rpdev;
	struct starfive_rpmsg_mem *shm_mem = &rpdev->shm_mem;
	struct device *dev = rpdev->cl.dev;
	struct starfive_vq_priv *priv;
	struct virtqueue *vq;

	if (!name)
		return NULL;
	/*
	 * Create the new vq, and tell virtio we're not interested in
	 * the 'weak' smp barriers, since we're talking with a real device.
	 */
	vq  = vring_new_virtqueue_with_init(id, RPMSG_NUM_BUFS / 2, 64, vdev, false, ctx,
		shm_mem->vaddr + shm_mem->count * VRING_BUFFER_SIZE,
		starfive_virtio_notify, callback, name,
		starfive_vring_init);
	if (!vq) {
		dev_err(dev, "vring_new_virtqueue %s failed\n", name);
		return ERR_PTR(-ENOMEM);
	}
	virdev->vring[id] = shm_mem->vaddr + shm_mem->count * VRING_BUFFER_SIZE;
	priv = &rpdev->vq_priv[shm_mem->count];
	priv->rpdev = rpdev;
	virdev->vq[id] = vq;
	priv->vq_id = virdev->base_vq_id + id;
	vq->priv = priv;
	shm_mem->count++;

	return vq;
}

static void starfive_virtio_del_vqs(struct virtio_device *vdev)
{
	struct virtqueue *vq, *n;
	struct rproc_vring *rvring;

	list_for_each_entry_safe(vq, n, &vdev->vqs, list) {
		rvring = vq->priv;
		rvring->vq = NULL;
		vring_del_virtqueue(vq);
	}
}

static int starfive_virtio_find_vqs(struct virtio_device *vdev, unsigned int nvqs,
				 struct virtqueue *vqs[],
				 vq_callback_t *callbacks[],
				 const char *const names[],
				 const bool *ctx,
				 struct irq_affinity *desc)
{
	int i, ret, queue_idx = 0;
	struct starfive_virdev *virdev = vdev_to_virdev(vdev);

	for (i = 0; i < nvqs; ++i) {
		if (!names[i]) {
			vqs[i] = NULL;
			continue;
		}

		vqs[i] = rp_find_vq(vdev, queue_idx++, callbacks[i], names[i],
				    ctx ? ctx[i] : false);
		if (IS_ERR(vqs[i])) {
			ret = PTR_ERR(vqs[i]);
			goto error;
		}
	}
	virdev->num_of_vqs = nvqs;

	return 0;

error:
	starfive_virtio_del_vqs(vdev);
	return ret;
}

static void starfive_virtio_reset(struct virtio_device *vdev)
{
}

static u8 starfive_virtio_get_status(struct virtio_device *vdev)
{
	struct starfive_virdev *virdev = vdev_to_virdev(vdev);

	return virdev->status;
}

static void starfive_virtio_set_status(struct virtio_device *vdev, u8 status)
{
	struct starfive_virdev *virdev = vdev_to_virdev(vdev);

	virdev->status = status;
}

static u64 starfive_virtio_get_features(struct virtio_device *vdev)
{
	return (BIT(0) | BIT(VIRTIO_F_VERSION_1));
}

static int starfive_virtio_finalize_features(struct virtio_device *vdev)
{
	vdev->features |= (BIT(0) | BIT(VIRTIO_F_VERSION_1));

	return 0;
}

bool starfive_get_shm_region(struct virtio_device *vdev,
			     struct virtio_shm_region *region, u8 id)
{
	struct starfive_virdev *virtdev = vdev_to_virdev(vdev);
	struct starfive_rpmsg_mem *shm_mem = &virtdev->rpdev->shm_mem;

	region->len = RPMSG_NUM_BUFS * MAX_RPMSG_BUF_SIZE;
	region->addr = (shm_mem->virtio_ram + virtdev->index * region->len);

	return true;
}

static const struct virtio_config_ops rpmsg_virtio_config_ops = {
	.get_features	= starfive_virtio_get_features,
	.finalize_features = starfive_virtio_finalize_features,
	.find_vqs	= starfive_virtio_find_vqs,
	.del_vqs	= starfive_virtio_del_vqs,
	.reset		= starfive_virtio_reset,
	.set_status	= starfive_virtio_set_status,
	.get_status	= starfive_virtio_get_status,
	.get_shm_region = starfive_get_shm_region,
};

static int starfive_alloc_memory_region(struct starfive_rpmsg_dev *rpmsg,
					struct platform_device *pdev)
{
	struct starfive_rpmsg_mem *shm_mem;
	struct resource *r;
	void *addr;
	int i;

	for (i = 0; i < 2; i++) {
		r = platform_get_resource(pdev, IORESOURCE_MEM, i);
		shm_mem = &rpmsg->shm_mem;
		if (i == 0) {
			shm_mem->phy_addr = r->start;
			shm_mem->size = resource_size(r);
			shm_mem->vaddr = devm_memremap(&pdev->dev, shm_mem->phy_addr,
				      shm_mem->size,
				      MEMREMAP_WB);
			if (IS_ERR(shm_mem->vaddr)) {
				dev_err(&pdev->dev, "unable to map memory region: %llx %d\n",
					(u64)r->start, shm_mem->size);
				return -EBUSY;
			}
		} else {
			addr = devm_memremap(&pdev->dev, r->start,
					     resource_size(r),
					     MEMREMAP_WB);
			if (IS_ERR(addr)) {
				dev_err(&pdev->dev, "unable to map virtio memory region: %llx %d\n",
					(u64)r->start, shm_mem->size);
				return -EBUSY;
			}
			shm_mem->virtio_ram = (unsigned long)addr;
		}

	}

	return 0;
}

static int starfive_rpmsg_xtr_channel_init(struct starfive_rpmsg_dev *rpdev)
{
	struct platform_device *pdev = rpdev->pdev;
	struct device *dev = &pdev->dev;
	struct mbox_client *cl;
	int ret = 0;

	cl = &rpdev->cl;
	cl->dev = dev;
	cl->tx_block = false;
	cl->tx_tout = 20;
	cl->knows_txdone = false;
	cl->rx_callback = starfive_rpmsg_rx_callback;

	rpdev->tx_ch = mbox_request_channel_byname(cl, "tx");
	if (IS_ERR(rpdev->tx_ch)) {
		ret = PTR_ERR(rpdev->tx_ch);
		dev_info(cl->dev, "failed to request mbox tx chan, ret %d\n",
			ret);
		goto err_out;
	}
	rpdev->rx_ch = mbox_request_channel_byname(cl, "rx");
	if (IS_ERR(rpdev->rx_ch)) {
		ret = PTR_ERR(rpdev->rx_ch);
		dev_info(cl->dev, "failed to request mbox rx chan, ret %d\n",
			ret);
		goto err_out;
	}

	return ret;

err_out:
	if (!IS_ERR(rpdev->tx_ch))
		mbox_free_channel(rpdev->tx_ch);
	if (!IS_ERR(rpdev->rx_ch))
		mbox_free_channel(rpdev->rx_ch);

	return ret;
}

static int starfive_rpmsg_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct starfive_rpmsg_dev *rpmsg_dev;
	int ret, i;
	char *buf;

	/* Allocate virtio device */
	rpmsg_dev = devm_kzalloc(dev, sizeof(*rpmsg_dev), GFP_KERNEL);
	if (!rpmsg_dev)
		return -ENOMEM;

	buf = devm_kzalloc(dev, PAGE_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	rpmsg_dev->rx_buffer.buf = buf;
	rpmsg_dev->rx_buffer.head = 0;
	rpmsg_dev->rx_buffer.tail = 0;
	spin_lock_init(&rpmsg_dev->mu_lock);

	rpmsg_dev->pdev = pdev;
	ret = starfive_alloc_memory_region(rpmsg_dev, pdev);
	if (ret)
		return ret;
	ret = of_property_read_u32(np, "vdev-nums", &rpmsg_dev->vdev_nums);
	if (ret)
		rpmsg_dev->vdev_nums = 1;

	ret = starfive_rpmsg_xtr_channel_init(rpmsg_dev);
	if (ret)
		return ret;

	INIT_DELAYED_WORK(&(rpmsg_dev->rpmsg_work), rpmsg_work_handler);

	memset(rpmsg_dev->shm_mem.vaddr, 0, rpmsg_dev->shm_mem.size);

	for (i = 0; i < rpmsg_dev->vdev_nums; i++) {
		rpmsg_dev->ivdev[i].vdev.id.device = VIRTIO_ID_RPMSG;
		rpmsg_dev->ivdev[i].vdev.config = &rpmsg_virtio_config_ops;
		rpmsg_dev->ivdev[i].vdev.dev.parent = &pdev->dev;
		rpmsg_dev->ivdev[i].base_vq_id = i * 2;
		rpmsg_dev->ivdev[i].rpdev = rpmsg_dev;
		rpmsg_dev->ivdev[i].index = i;
		ret = register_virtio_device(&rpmsg_dev->ivdev[i].vdev);
		if (ret) {
			dev_err(dev, "failed to register vdev: %d\n", ret);
			if (i == 0)
				goto err_chl;
			break;
		}
	}

	platform_set_drvdata(pdev, rpmsg_dev);

	dev_info(dev, "registered %s\n", dev_name(&pdev->dev));

	return 0;
err_chl:
	if (!IS_ERR(rpmsg_dev->tx_ch))
		mbox_free_channel(rpmsg_dev->tx_ch);
	if (!IS_ERR(rpmsg_dev->rx_ch))
		mbox_free_channel(rpmsg_dev->rx_ch);
	return ret;
}

static int starfive_rpmsg_remove(struct platform_device *pdev)
{
	struct starfive_rpmsg_dev *vdev = platform_get_drvdata(pdev);
	int i;

	cancel_delayed_work_sync(&vdev->rpmsg_work);
	for (i = 0; i < vdev->vdev_nums; i++)
		unregister_virtio_device(&vdev->ivdev[i].vdev);

	if (!IS_ERR(vdev->tx_ch))
		mbox_free_channel(vdev->tx_ch);
	if (!IS_ERR(vdev->rx_ch))
		mbox_free_channel(vdev->rx_ch);

	return 0;
}

static const struct of_device_id amp_rpmsg_of_match[] = {
	{ .compatible = "starfive,amp-virtio-rpmsg", .data = NULL },
	{},
};
MODULE_DEVICE_TABLE(of, amp_rpmsg_of_match);

static struct platform_driver starfive_rmpsg_driver = {
	.probe = starfive_rpmsg_probe,
	.remove = starfive_rpmsg_remove,
	.driver = {
		.name = "starfive-rpmsg",
		.of_match_table = amp_rpmsg_of_match,
	},
};
module_platform_driver(starfive_rmpsg_driver);
MODULE_LICENSE("GPL v2");
