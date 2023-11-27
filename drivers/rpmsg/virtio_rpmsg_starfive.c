#include <linux/dma-map-ops.h>
#include <linux/export.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
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


struct starfive_rpmsg_mem {
	void __iomem *vaddr;
	phys_addr_t phy_addr;
	u32 vq_offset;
	u32 queue_base;
	size_t size;
};

struct starfive_rpmsg_vq {
	struct virtqueue *vq[2];
};

struct starfive_rpmsg_dev {
	struct device *dev;
	struct virtio_device vdev;
	struct starfive_rpmsg_mem shm_mem;
	struct work_struct rx_work;
	struct work_struct tx_work;
	unsigned long rx_chan_bits;
	unsigned long tx_chan_bits;
	int rtos_hart_id;
	u8 status;
};

#define MAX_CH (16)

struct gen_sw_mbox
{
    uint32_t rx_status[MAX_CH];
    uint32_t tx_status[MAX_CH];
    uint32_t reserved[MAX_CH];
    uint32_t rx_ch[MAX_CH];
    uint32_t tx_ch[MAX_CH];
};

enum sw_mbox_channel_status
{
    S_READY,
    S_BUSY,
    S_DONE,
};

struct starfive_rpmsg_vq rpmsg_vq;

extern int sbi_send_ipi_amp(unsigned int hartid);

static inline struct starfive_rpmsg_dev *vdev_to_rpdev(struct virtio_device *dev)
{
	return container_of(dev, struct starfive_rpmsg_dev, vdev);
}

static void rpmsg_rx_work_func(struct work_struct *work)
{
	struct starfive_rpmsg_dev *rpdev = container_of(work, struct starfive_rpmsg_dev, rx_work);
	struct gen_sw_mbox *base = (void *)rpdev->shm_mem.vaddr;
	int chan;

	for_each_set_bit(chan, &rpdev->rx_chan_bits, MAX_CH) {
		if (base->rx_status[chan] == S_DONE) {
			vring_interrupt(0, rpmsg_vq.vq[0]);
		}
		clear_bit(chan, &rpdev->rx_chan_bits);
	}
}

static void rpmsg_tx_work_func(struct work_struct *work)
{
	struct starfive_rpmsg_dev *rpdev = container_of(work, struct starfive_rpmsg_dev, tx_work);
	struct gen_sw_mbox *base = (void *)rpdev->shm_mem.vaddr;
	int chan;

	for_each_set_bit(chan, &rpdev->tx_chan_bits, MAX_CH) {
		if (base->tx_status[chan] == S_DONE) {
			base->tx_status[chan] = S_READY;
			vring_interrupt(0, rpmsg_vq.vq[1]);
		}
		clear_bit(chan, &rpdev->tx_chan_bits);
	}
}

void rpmsg_handler(void *param) /* maybe put this handler to thread */
{
	struct virtqueue *vq[2];
	struct starfive_rpmsg_dev *rpdev;
	struct gen_sw_mbox *base;
	int tasklet_sch = 0;

	vq[0] = rpmsg_vq.vq[0]; /* rx queue */
	rpdev = (void *)vq[0]->priv;
	base = (void *)rpdev->shm_mem.vaddr;

	if (base->tx_status[0] == S_DONE) {
		set_bit(0, &rpdev->tx_chan_bits);
		schedule_work(&rpdev->tx_work);
	}

	/* Check if the interrupt is for us */
	if (base->rx_status[0] != S_BUSY) {
		return;
	}

	base->rx_status[0] = S_DONE;

	set_bit(0, &rpdev->rx_chan_bits);
	schedule_work(&rpdev->rx_work);
}

static inline uint32_t starfive_vring_init(struct vring *vr, unsigned int num, void *p,
			      unsigned long align)

{
	unsigned long addr;
	uint32_t offset;

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
	addr += (64 - 1UL);
	addr &= ~(align - 1UL);

	return addr - (unsigned long) vr->desc;
}

static bool starfive_virtio_notify(struct virtqueue *vq)
{
	struct starfive_rpmsg_dev *rpdev = (void *)vq->priv;
	struct gen_sw_mbox *base = (void *)rpdev->shm_mem.vaddr;

	if (!strcmp(vq->name, "output"))
		base->tx_status[0] = S_BUSY;

	/* sync before trigger interrupt to remote */

	mb();

	sbi_send_ipi_amp(rpdev->rtos_hart_id);
	return true;
}

static struct virtqueue *rp_find_vq(struct virtio_device *vdev,
				    unsigned int id,
				    void (*callback)(struct virtqueue *vq),
				    const char *name, bool ctx)
{
	struct starfive_rpmsg_dev *rpdev = vdev_to_rpdev(vdev);
	struct starfive_rpmsg_mem *shm_mem = &rpdev->shm_mem;
	struct device *dev = rpdev->dev;
	unsigned long queue_base;
	struct virtqueue *vq;
	struct vring vring;
	void *addr;
	int len, size;

	if (!name)
		return NULL;
	/*
	 * Create the new vq, and tell virtio we're not interested in
	 * the 'weak' smp barriers, since we're talking with a real device.
	 */

	if (virtio_has_feature(vdev, VIRTIO_F_RING_PACKED))
		return NULL;

	shm_mem->queue_base =
		starfive_vring_init(&vring, 2, shm_mem->vaddr + shm_mem->vq_offset + shm_mem->queue_base, 64);

	vq  =__vring_new_virtqueue(id, vring, vdev, false, ctx,
				     starfive_virtio_notify, callback, name);
	if (!vq) {
		dev_err(dev, "vring_new_virtqueue %s failed\n", name);
		return ERR_PTR(-ENOMEM);
	}

	vq->priv = rpdev;
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
	rpmsg_vq.vq[0] = rpmsg_vq.vq[1] = NULL;
}

static int starfive_virtio_find_vqs(struct virtio_device *vdev, unsigned int nvqs,
				 struct virtqueue *vqs[],
				 vq_callback_t *callbacks[],
				 const char * const names[],
				 const bool * ctx,
				 struct irq_affinity *desc)
{
	int i, ret, queue_idx = 0;

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
		rpmsg_vq.vq[i] = vqs[i];
	}

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
	struct starfive_rpmsg_dev *rpmsg = vdev_to_rpdev(vdev);

	return rpmsg->status;
}

static void starfive_virtio_set_status(struct virtio_device *vdev, u8 status)
{
	struct starfive_rpmsg_dev *rpmsg = vdev_to_rpdev(vdev);

	rpmsg->status = status;
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

static const struct virtio_config_ops rpmsg_virtio_config_ops = {
	.get_features	= starfive_virtio_get_features,
	.finalize_features = starfive_virtio_finalize_features,
	.find_vqs	= starfive_virtio_find_vqs,
	.del_vqs	= starfive_virtio_del_vqs,
	.reset		= starfive_virtio_reset,
	.set_status	= starfive_virtio_set_status,
	.get_status	= starfive_virtio_get_status,
};

static int starfive_alloc_memory_region(struct starfive_rpmsg_dev *rpmsg)
{
	struct starfive_rpmsg_mem *shm_mem;
        struct device_node *node;
        struct resource r;
        int ret;

        node = of_parse_phandle(rpmsg->dev->of_node, "memory-region", 0);
        if (!node) {
                dev_err(rpmsg->dev, "no memory-region specified\n");
                return -EINVAL;
        }

        ret = of_address_to_resource(node, 0, &r);
        if (ret) {
		dev_err(rpmsg->dev, "map to  resource fail\n");
                return ret;
        }

	shm_mem = &rpmsg->shm_mem;
        shm_mem->phy_addr = r.start;
        shm_mem->size = resource_size(&r);
	shm_mem->vaddr = devm_memremap(rpmsg->dev, shm_mem->phy_addr,
				      shm_mem->size,
				      MEMREMAP_WB);

        if (IS_ERR(shm_mem->vaddr)) {
                dev_err(rpmsg->dev, "unable to map memory region: %pa+%zx\n",
                        &r.start, shm_mem->size);
                return -EBUSY;
        }
        return 0;
}

static int starfive_rpmsg_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct starfive_rpmsg_dev *rpmsg_dev;
	struct virtio_device *vdev;
	int ret;

	/* Allocate virtio device */
	rpmsg_dev = devm_kzalloc(dev, sizeof(*rpmsg_dev), GFP_KERNEL);
	if (!rpmsg_dev) {
		ret = -ENOMEM;
		goto out;
	}

	rpmsg_dev->dev = dev;
	ret = starfive_alloc_memory_region(rpmsg_dev);
	if (ret)
		return ret;
	rpmsg_dev->shm_mem.vq_offset = 0x10000;
	rpmsg_dev->rtos_hart_id = 0x4;

	memset(rpmsg_dev->shm_mem.vaddr + rpmsg_dev->shm_mem.vq_offset, 0, 0x10000);
	vdev = &rpmsg_dev->vdev;
	vdev->id.device	= VIRTIO_ID_RPMSG;
	vdev->config = &rpmsg_virtio_config_ops;
	vdev->dev.parent = dev;
	INIT_WORK(&rpmsg_dev->tx_work, rpmsg_tx_work_func);
	INIT_WORK(&rpmsg_dev->rx_work, rpmsg_rx_work_func);

	ret = register_virtio_device(vdev);
	if (ret) {
		dev_err(dev, "failed to register vdev: %d\n", ret);
		goto out;
	}
	platform_set_drvdata(pdev, rpmsg_dev);

	dev_info(dev, "registered %s\n", dev_name(&vdev->dev));

out:
	return ret;
}

static int starfive_rpmsg_remove(struct platform_device *pdev)
{
	struct starfive_rpmsg_dev *vdev = platform_get_drvdata(pdev);

	cancel_work_sync(&vdev->rx_work);
	cancel_work_sync(&vdev->tx_work);
	unregister_virtio_device(&vdev->vdev);
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
