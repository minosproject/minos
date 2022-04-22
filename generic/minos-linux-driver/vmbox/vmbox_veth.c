/*
 * Copyright (C) 2019 Min Le (lemin9538@gmail.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/interrupt.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/crc32.h>
#include <linux/mii.h>
#include <linux/of.h>
#include <linux/of_net.h>
#include <linux/ethtool.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/slab.h>

#include <asm/delay.h>
#include <asm/irq.h>
#include <asm/io.h>

#include "vmbox_bus.h"
#include "../minos.h"
#include <linux/skbuff.h>

#define VMBOX_VETH_PKT_MAX	2048
#define VMBOX_VETH_ZLEN		64

enum vmbox_veth_state {
	VETH_STAT_CLOSED,
	VETH_STAT_OPENED,
	VETH_STAT_LINKUP,
};

struct vmbox_veth {
	u32 msg_enable;
	u32 wake_state;
	struct net_device *ndev;
	struct vmbox_device *vdev;
	struct vmbox_virtqueue *rx_vq;
	struct vmbox_virtqueue *tx_vq;
	spinlock_t rx_lock;
	spinlock_t tx_lock;
};

static inline void
copy_data_to_skb(struct vmbox_vring_buf *buf, struct sk_buff *skb)
{
	memcpy(skb_put(skb, buf->size), buf->addr, buf->size);
}

static inline void
copy_iodata_to_skb(struct vmbox_vring_buf *buf, struct sk_buff *skb)
{
	memcpy_fromio(skb_put(skb, buf->size), buf->addr, buf->size);
}

static inline void
copy_data_from_skb(struct vmbox_vring_buf *buf, struct sk_buff *skb)
{
	memcpy(buf->addr, skb->data, skb->len);
	buf->size = skb->len;
}

static inline void
copy_iodata_from_skb(struct vmbox_vring_buf *buf, struct sk_buff *skb)
{
	memcpy_toio(buf->addr, skb->data, skb->len);
	buf->size = skb->len;
}

#define vmbox_dev_to_veth(vdev)	(struct vmbox_veth *)vmbox_get_drvdata(vdev)

static int vmbox_veth_start_xmit(struct sk_buff *skb,
		struct net_device *ndev)
{
	int ret;
	unsigned long flags;
	struct vmbox_vring_buf buf;
	struct vmbox_veth *veth = netdev_priv(ndev);

	if (!vmbox_device_otherside_open(veth->vdev)) {
		dev_err(&ndev->dev, "vmbox veth backend not ready\n");
		goto out;
	}

	if (skb_put_padto(skb, VMBOX_VETH_ZLEN)) {
		ndev->stats.tx_errors++;
		return NETDEV_TX_OK;
	}

	if (unlikely(skb->len > VMBOX_VETH_PKT_MAX)) {
		dev_warn(&ndev->dev, "packet size too big\n");
		goto out;
	}

	ret = vmbox_virtq_get_vring_buf(veth->tx_vq, &buf, 1);
	if (ret) {
		pr_err("vmbox veth can not send skb\n");
		netif_stop_queue(ndev);
		return NETDEV_TX_BUSY;
	}

	copy_iodata_from_skb(&buf, skb);

	spin_lock_irqsave(&veth->tx_lock, flags);
	ret = vmbox_virtq_add_outbuf(veth->tx_vq, &buf, 1);
	spin_unlock_irqrestore(&veth->tx_lock, flags);
	if (ret < 0)
		pr_err("vmbox veth send skb failed\n");
	else
		vmbox_virtq_notify_avail(veth->tx_vq);

	ndev->stats.tx_bytes += skb->len;

out:
	dev_consume_skb_any(skb);
	return NETDEV_TX_OK;
}

static void vmbox_veth_timeout(struct net_device *ndev)
{
	dev_warn(&ndev->dev, "vmbox veth tx timeout\n");
}

static void vmbox_veth_hash_table(struct net_device *ndev)
{

}

static int vmbox_veth_ioctl(struct net_device *ndev,
		struct ifreq *req, int cmd)
{
	return 0;
}

static int vmbox_veth_set_features(struct net_device *ndev,
		netdev_features_t feature)
{
	return 0;
}

static int vmbox_veth_open(struct net_device *ndev)
{
	struct vmbox_veth *veth = netdev_priv(ndev);

	if (netif_msg_ifup(veth))
		dev_info(&ndev->dev, "enbaling vmbox veth\n");

	vmbox_virtq_startup(veth->rx_vq);
	vmbox_virtq_startup(veth->tx_vq);

	enable_irq(veth->vdev->vring_irq);
	vmbox_device_state_change(veth->vdev, VMBOX_DEV_STAT_OPENED);

	if (netif_queue_stopped(ndev)) {
		dev_dbg(&ndev->dev, " resuming queue\n");
		netif_wake_queue(ndev);
	} else {
		dev_dbg(&ndev->dev, " starting queue\n");
		netif_start_queue(ndev);
	}

	return 0;
}

static int vmbox_veth_stop(struct net_device *ndev)
{
	struct vmbox_veth *veth = netdev_priv(ndev);

	vmbox_device_state_change(veth->vdev, VMBOX_DEV_STAT_CLOSED);

	vmbox_virtq_shutdown(veth->rx_vq);
	vmbox_virtq_shutdown(veth->tx_vq);
	disable_irq(veth->vdev->vring_irq);

	if (!netif_queue_stopped(ndev))
		netif_stop_queue(veth->ndev);

	return 0;
}

static const struct net_device_ops vmbox_netdev_ops = {
	.ndo_open		= vmbox_veth_open,
	.ndo_stop		= vmbox_veth_stop,
	.ndo_start_xmit		= vmbox_veth_start_xmit,
	.ndo_tx_timeout		= vmbox_veth_timeout,
	.ndo_set_rx_mode	= vmbox_veth_hash_table,
	.ndo_do_ioctl		= vmbox_veth_ioctl,
	.ndo_set_features	= vmbox_veth_set_features,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_set_mac_address	= eth_mac_addr,
};

static u32 vmbox_get_msglevel(struct net_device *ndev)
{
	struct vmbox_veth *veth = netdev_priv(ndev);

	return veth->msg_enable;
}

static void vmbox_set_msglevel(struct net_device *ndev, u32 value)
{
	struct vmbox_veth *veth = netdev_priv(ndev);

	veth->msg_enable = value;
}

static const struct ethtool_ops vmbox_ethtool_ops = {
	.get_link	= ethtool_op_get_link,
	.set_msglevel	= vmbox_set_msglevel,
	.get_msglevel	= vmbox_get_msglevel,
};

static void vmbox_veth_get_macaddr(struct vmbox_veth *veth)
{
	u8 *base = veth->ndev->dev_addr;

	eth_random_addr(base);
	pr_info("vmbox_veth: MAC addr %2x:%2x:%2x:%2x:%2x:%2x\n",
			base[0], base[1], base[2],
			base[3], base[4], base[5]);
}

static int vmbox_veth_rx_cb(struct vmbox_virtqueue *vq)
{
	int ret;
	unsigned long flags;
	struct sk_buff *skb;
	struct vmbox_vring_buf buf;
	struct vmbox_device *vdev = (struct vmbox_device *)vq->pdata;
	struct vmbox_veth *veth = vmbox_dev_to_veth(vdev);
	struct net_device *ndev = veth->ndev;

	/*
	 * get the skb buffer from the avail virtq and copy
	 * the data from the buffer then send to the stack
	 */
	spin_lock_irqsave(&veth->rx_lock, flags);
	while (vmbox_virtq_more_avail(vq)) {
		ret = vmbox_virtq_get_avail_buf(vq, &buf);
		if (ret)
			goto out;

		skb = netdev_alloc_skb(veth->ndev, buf.size);
		if (likely(skb)) {
			copy_data_to_skb(&buf, skb);
			skb->protocol = eth_type_trans(skb, ndev);
			ndev->stats.rx_packets++;
			ndev->stats.rx_bytes += buf.size;
			netif_rx(skb);
		} else {
			if (net_ratelimit())
				dev_warn(&ndev->dev,
					"low on memory - packet dropped\n");
			ndev->stats.rx_dropped++;
		}

		vmbox_virtq_add_used_and_signal(vq, ret, 0);
	}
out:
	spin_unlock_irqrestore(&veth->rx_lock, flags);
	return 0;
}

static int vmbox_veth_tx_cb(struct vmbox_virtqueue *vq)
{
	int ret;
	unsigned long flags;
	struct vmbox_veth *veth =
		vmbox_dev_to_veth((struct vmbox_device *)vq->pdata);
	struct vmbox_vring_buf buf;

	spin_lock_irqsave(&veth->tx_lock, flags);
	while (vmbox_virtq_more_used(vq)) {
		ret = vmbox_virtq_get_used_buf(vq, &buf);
		if (ret)
			break;

		vmbox_virtq_detach_buf(vq, &buf, 1);
		veth->ndev->stats.tx_packets++;
		pr_debug("veth: recall one buffer %d\n", buf.id);
	}
	spin_unlock_irqrestore(&veth->tx_lock, flags);

	return 0;
}

static void vmbox_veth_setup_vq(struct vmbox_device *vdev)
{
	struct vmbox_veth *veth = vmbox_get_drvdata(vdev);

	if (vmbox_device_is_backend(vdev)) {
		veth->tx_vq = vdev->vqs[1];
		veth->rx_vq = vdev->vqs[0];
	} else {
		veth->tx_vq = vdev->vqs[0];
		veth->rx_vq = vdev->vqs[1];
	}

	veth->rx_vq->callback = vmbox_veth_rx_cb;
	veth->tx_vq->callback = vmbox_veth_tx_cb;
	pr_info("veth: tx_vq [%d] rx_vq [%d]\n",
			veth->tx_vq->index, veth->rx_vq->index);
}

static int vmbox_veth_state_change(struct vmbox_device *dev, int state)
{
	struct vmbox_veth *veth = vmbox_get_drvdata(dev);

	switch (state) {
	case VMBOX_DEV_STAT_OPENED:
		if (!vmbox_device_is_backend(dev)) {
			netif_carrier_on(veth->ndev);
			dev_info(&veth->ndev->dev, "%s: link up, 1000Mbps\n",
					veth->ndev->name);
		}
		break;
	case VMBOX_DEV_STAT_CLOSED:
		if (!vmbox_device_is_backend(dev)) {
			netif_carrier_off(veth->ndev);
			dev_info(&veth->ndev->dev, "%s: link down\n",
					veth->ndev->name);
		}
		break;
	default:
		break;
	}
	return 0;
}

static int vmbox_veth_probe(struct vmbox_device *vdev)
{
	int ret = 0;
	struct vmbox_veth *veth;
	struct net_device *ndev;

	pr_info("vmbox veth driver probe\n");

	ndev = alloc_etherdev(sizeof(struct vmbox_veth));
	if (!ndev) {
		ret = -ENOMEM;
		goto release_veth;
	}

	SET_NETDEV_DEV(ndev, &vdev->dev);
	veth = netdev_priv(ndev);

	veth->vdev = vdev;
	veth->ndev = ndev;
	spin_lock_init(&veth->rx_lock);
	spin_lock_init(&veth->tx_lock);

	ndev->netdev_ops = &vmbox_netdev_ops;
	ndev->watchdog_timeo = msecs_to_jiffies(5000);
	ndev->ethtool_ops = &vmbox_ethtool_ops;

	vmbox_set_drvdata(vdev, veth);
	vmbox_device_remap(vdev);

	vmbox_veth_get_macaddr(veth);

	vmbox_device_init(vdev, 0);
	vmbox_device_online(vdev);
	disable_irq(vdev->vring_irq);

	if (!vmbox_device_is_backend(vdev))
		netif_carrier_off(ndev);

	ret = register_netdev(ndev);
	if (ret)
		goto release_veth;

	return 0;

release_veth:
	kfree(ndev);

	return -ENOMEM;
}

static void vmbox_veth_remove(struct vmbox_device *vdev)
{
	struct net_device *ndev = vmbox_get_drvdata(vdev);

	vmbox_device_offline(vdev);
	unregister_netdev(ndev);
	free_netdev(ndev);
	vmbox_device_unmap(vdev);
}

static struct vmbox_device_id vmbox_veth_ids[] = {
	{0x3430, VMBOX_ANY_VENDOR_ID},
	{0x3431, VMBOX_ANY_VENDOR_ID},
	{}
};

static struct vmbox_driver vmbox_veth_drv = {
	.id_table 		= vmbox_veth_ids,
	.probe 			= vmbox_veth_probe,
	.remove			= vmbox_veth_remove,
	.otherside_state_change = vmbox_veth_state_change,
	.setup_vq 		= vmbox_veth_setup_vq,
	.driver = {
		.name = "vmbox-veth",
	},
};

module_vmbox_driver(vmbox_veth_drv);
MODULE_LICENSE("GPL");
