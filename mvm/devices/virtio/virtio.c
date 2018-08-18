/*
 * Copyright (C) 2018 Min Le (lemin9538@gmail.com)
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

#include <mvm.h>
#include <virtio.h>
#include <virtio_mmio.h>
#include <io.h>

static void *hv_create_virtio_device(struct vm *vm)
{
	int ret;
	void *iomem;

	ret = ioctl(vm->vm_fd, IOCTL_CREATE_VIRTIO_DEVICE, &iomem);
	if (ret) {
		printf("create virtio device failed %d\n", ret);
		return NULL;
	}

	return iomem;
}

static inline int next_desc(struct vring_desc *desc)
{
	return (!(desc->flags & VRING_DESC_F_NEXT)) ? -1 : desc->next;
}

static void translate_desc(struct vring_desc *desc, int index,
		struct iovec *iov, int iov_size)
{
	if (index >= iov_size) {
		pr_err("index %d out of iov range %d\n", index, iov_size);
		return;
	}

	iov[index].iov_len = desc->len;
	iov[index].iov_base = (void *)gpa_to_hvm_va(desc->addr);
}

static int get_indirect_buf(struct vring_desc *desc, int index,
		struct iovec *iov, int iov_size)
{
	struct vring_desc *in_desc, *vd;
	unsigned int nr_in, old_index = index;
	unsigned int next = 0;

	nr_in = desc->len / 16;
	if ((desc->len & 0xf) || nr_in == 0) {
		pr_err("invalid indirect len 0x%x\n", desc->len);
		return -EINVAL;
	}

	in_desc = (struct vring_desc *)gpa_to_hvm_va(desc->addr);

	for (;;) {
		vd = &in_desc[next];
		if (vd->flags & VRING_DESC_F_INDIRECT) {
			pr_err("invalid desc in indirect desc\n");
			return -EINVAL;
		}

		translate_desc(desc, index, iov, iov_size);
		index++;

		if (index > iov_size) {
			pr_err("%d out of ivo size\n", index);
			return -ENOMEM;
		}

		next = next_desc(desc);
		if (next >= nr_in) {
			pr_err("out of indirect desc range\n");
			return -EINVAL;
		}

		if (next == -1)
			break;
	}

	return (index - old_index);
}

static void virtq_update_used_flags(struct virt_queue *vq)
{
	vq->used->flags = vq->used_flags;
}

static void virtq_update_avail_event(struct virt_queue *vq, uint16_t event)
{
	*virtq_avail_event(vq) = event;
}

int virtq_enable_notify(struct virt_queue *vq)
{
	uint16_t avail_idx;

	if (!(vq->used_flags & VRING_USED_F_NO_NOTIFY))
		return 0;

	vq->used_flags &= ~VRING_USED_F_NO_NOTIFY;
	if (!virtq_has_feature(vq, VIRTIO_RING_F_EVENT_IDX)) {
		virtq_update_used_flags(vq);
	} else
		virtq_update_avail_event(vq, vq->avail_idx);

	avail_idx = vq->avail->idx;

	return avail_idx != vq->avail_idx;
}

/* tell the guest do not notify again */
void virtq_disable_notify(struct virt_queue *vq)
{
	if (vq->used_flags & VRING_USED_F_NO_NOTIFY)
		return;

	vq->used_flags |= VRING_USED_F_NO_NOTIFY;
	if (virtq_has_feature(vq, VIRTIO_RING_F_EVENT_IDX))
		virtq_update_used_flags(vq);
}

int virtq_get_descs(struct virt_queue *vq,
		struct iovec *iov, unsigned int iov_size,
		unsigned int *in_num, unsigned int *out_num)
{
	struct vring_desc *desc;
	unsigned int i, head;
	uint16_t last_avail_idx;
	uint16_t avail_idx;
	int iov_index = 0, count, ret;

	last_avail_idx = vq->last_avail_idx;
	avail_idx = vq->avail->idx;
	vq->avail_idx = avail_idx;

	count = avail_idx - last_avail_idx;
	if (count == 0)
		return -EINVAL;

	if (count > vq->num) {
		pr_err("avail ring out of range\n");
		return -EINVAL;
	}

	head = vq->avail->ring[last_avail_idx & (vq->num - 1)];
	if (head >= vq->num) {
		pr_err("avail ring idx out of range\n");
		return -EINVAL;
	}

	*in_num = *out_num = 0;

	i = head;
	do {
		if (iov_index >= iov_size) {
			pr_err("iov count out of iov range %d\n", iov_size);
			return -ENOMEM;
		}

		if (i >= vq->num) {
			pr_err("desc index %d > %d head = %d\n",
					i, vq->num, head);
			return -EINVAL;
		}

		desc = &vq->desc[i];
		if (desc->flags & VRING_DESC_F_INDIRECT) {
			ret = get_indirect_buf(desc, iov_index, iov, iov_size);
			if (ret < 0) {
				pr_err("failed to get indirect buf\n");
				return ret;
			}

			iov_index += ret;
			continue;
		}

		translate_desc(desc, iov_index, iov, iov_size);
		if (desc->flags & VRING_DESC_F_WRITE)
			*in_num += 1;
		else
			*out_num += 1;
		iov_index++;
	} while ((i = next_desc(desc)) != -1);

	vq->last_avail_idx++;

	return head;
}

void virtq_discard_desc(struct virt_queue *vq, int n)
{
	vq->last_avail_idx -= n;
}

static int __virtq_add_used_n(struct virt_queue *vq,
			struct vring_used_elem *heads,
			unsigned int count)
{
	struct vring_used_elem *used;
	uint16_t old, new;
	int start;

	start = vq->last_used_idx & vq->num;
	used = vq->used->ring + start;

	if (count == 1) {
		used->id = heads[0].id;
		used->id = heads[0].len;
	} else
		memcpy(used, heads, count * sizeof(*used));

	old = vq->last_used_idx;
	new = (vq->last_used_idx += count);

	if (((uint16_t)(new - vq->signalled_used)) < ((uint16_t)(new - old)))
		vq->signalled_used_valid = 0;

	return 0;
}

int virtq_add_used_n(struct virt_queue *vq,
			struct vring_used_elem *heads,
			unsigned int count)
{
	int start, n, r;

	start = vq->last_used_idx % vq->num;
	n = vq->num - start;
	if (n < count) {
		r = __virtq_add_used_n(vq, heads, n);
		if (r < 0)
			return r;
		heads += n;
		count -= n;
	}

	r = __virtq_add_used_n(vq, heads, count);

	vq->used->idx = vq->last_used_idx;

	return r;
}

int virtq_add_used(struct virt_queue *vq,
		unsigned int head, uint32_t len)
{
	struct vring_used_elem heads = {
		.id = head,
		.len = len,
	};

	return virtq_add_used_n(vq, &heads, 1);
}

int virtq_need_notify(struct virt_queue *vq)
{
	uint16_t old, new;
	uint16_t event, flags;
	int notify;

	if (virtq_has_feature(vq, VIRTIO_F_NOTIFY_ON_EMPTY) &&
			(vq->avail_idx == vq->last_avail_idx))
		return 1;

	if (!virtq_has_feature(vq, VIRTIO_RING_F_EVENT_IDX)) {
		flags = vq->avail->flags;;
		return (!(flags & VRING_AVAIL_F_NO_INTERRUPT));
	}

	old = vq->signalled_used;
	notify = vq->signalled_used_valid;
	new = vq->signalled_used = vq->last_used_idx;
	vq->signalled_used_valid = 1;

	if (!notify)
		return 1;

	event = *virtq_used_event(vq);

	return virtq_need_event(event, new, old);
}

void virtq_notify(struct virtio_device *dev, struct virt_queue *vq)
{
	if (virtq_need_notify(vq))
		virtio_send_irq(dev);
}

void virtq_add_used_and_signal(struct virtio_device *dev,
		struct virt_queue *vq, unsigned int head, int len)
{
	virtq_add_used(vq, head, len);
	virtq_notify(dev, vq);
}

void virtq_add_used_and_signal_n(struct virtio_device *dev,
				struct virt_queue *vq,
				struct vring_used_elem *heads,
				unsigned int count)
{
	virtq_add_used_n(vq, heads, count);
	virtq_notify(dev, vq);
}

static int __virtio_vdev_init(struct vdev *vdev,
		void *iomem, int type, int rs)
{
	void *base;

	vdev->iomem_physic = iomem;
	base = vdev_map_iomem(iomem, 4096);
	if (base == (void *)-1)
		return -ENOMEM;

	vdev->iomem = base;
	vdev->gvm_irq = ioread32(base + VIRTIO_MMIO_GVM_IRQ);
	vdev->hvm_irq = ioread32(base + VIRTIO_MMIO_HVM_IRQ);

	/* TO BE FIX need to covert to 64bit address */
	vdev->guest_iomem = (unsigned long)ioread32(base +
			VIRTIO_MMIO_GVM_ADDR);

	pr_debug("vdev : %d %d 0x%lx 0x%lx\n", vdev->gvm_irq,
			vdev->hvm_irq, (unsigned long)vdev->iomem,
			vdev->guest_iomem);

	if (rs > VIRTQUEUE_MAX_SIZE)
		rs = VIRTQUEUE_MAX_SIZE;

	iowrite32(base + VIRTIO_MMIO_MAGIC_VALUE, VIRTIO_MMIO_MAGIG);
	iowrite32(base + VIRTIO_MMIO_VERSION, VIRTIO_VERSION);
	iowrite32(base + VIRTIO_MMIO_VENDOR_ID, VIRTIO_VENDER_ID);
	iowrite32(base + VIRTIO_MMIO_DEVICE_ID, type);
	iowrite32(base + VIRTIO_MMIO_QUEUE_NUM_MAX, rs);

	return 0;

}

int virtio_device_init(struct virtio_device *virt_dev,
		struct vdev *vdev, int type, int queue_nr, int rs)
{
	void *iomem;
	int ret;

	if (!virt_dev || !vdev)
		return -EINVAL;

	if ((type == 0) || (type > 18) ||
			((type > 9) && (type < 18))) {
		pr_err("unsupport virtio device type %d\n", type);
		return -EINVAL;
	}

	iomem = hv_create_virtio_device(vdev->vm);
	if (!iomem)
		return -ENOMEM;

	ret = __virtio_vdev_init(vdev, iomem, type, rs);
	if (ret) {
		pr_err("failed to init virtio device\n");
		return ret;
	}

	virt_dev->vdev = vdev;
	virt_dev->config = iomem + VIRTIO_MMIO_CONFIG;

	/* alloc memory for virtio queue */
	virt_dev->vqs = malloc(sizeof(struct virt_queue) * queue_nr);
	if (!virt_dev->vqs) {
		ret = -ENOMEM;
		goto release_virtio_dev;
	}

	virt_dev->nr_vq = queue_nr;
	memset(virt_dev->vqs, 0, sizeof(struct virt_queue) * queue_nr);

	return 0;

release_virtio_dev:
	/* tbd */
	return ret;
}

static void inline virtio_hvm_ack(struct virtio_device *dev)
{
	iowrite32(dev->vdev->iomem + VIRTIO_MMIO_EVENT_ACK, 1);
}

static int virtio_status_event(struct virtio_device *dev, uint32_t arg)
{
	switch (arg) {
	case VIRTIO_DEV_NEEDS_RESET:
		break;

	case VIRTIO_DEV_STATUS_OK:
		break;
	}

	return 0;
}

static int virtio_queue_event(struct virtio_device *dev, uint32_t arg)
{
	struct virt_queue *queue;

	if (arg > dev->nr_vq) {
		pr_err("receive unvaild virt queue event %d\n", arg);
		return -EINVAL;
	}

	queue = &dev->vqs[arg];
	if (!queue->callback) {
		pr_warn("no callback for the queue %d\n", arg);
		return -ENOENT;
	}

	return queue->callback(dev, queue);
}

static int virtio_mmio_event(struct virtio_device *dev, uint32_t arg)
{
	return 0;
}

static int virtio_buffer_event(struct virtio_device *dev, uint32_t arg)
{
	struct virt_queue *queue;
	void *iomem = dev->vdev->iomem;
	uint32_t high, low;

	if (arg >= dev->nr_vq) {
		pr_err("invaild virt queue index %d\n", arg);
		return -EINVAL;
	}

	queue = &dev->vqs[arg];
	queue->queue_index = arg;
	queue->num = ioread32(iomem + VIRTIO_MMIO_QUEUE_NUM);

	high = ioread32(iomem + VIRTIO_MMIO_QUEUE_DESC_HIGH);
	low = ioread32(iomem + VIRTIO_MMIO_QUEUE_AVAIL_LOW);
	queue->desc = (struct vring_desc *)u32_to_u64(high, low);

	high = ioread32(iomem + VIRTIO_MMIO_QUEUE_AVAIL_HIGH);
	low = ioread32(iomem + VIRTIO_MMIO_QUEUE_AVAIL_LOW);
	queue->avail = (struct vring_avail *)u32_to_u64(high, low);

	high = ioread32(iomem + VIRTIO_MMIO_QUEUE_USED_HIGH);
	low = ioread32(iomem + VIRTIO_MMIO_QUEUE_USED_LOW);
	queue->used = (struct vring_used *)u32_to_u64(high, low);

	pr_debug("find virt_queue %d 0x%p 0x%p 0x%p\n", queue->num,
			queue->desc, queue->avail, queue->used);

	queue->desc = (struct vring_desc *)
		gpa_to_hvm_va((unsigned long)queue->desc);
	queue->avail = (struct vring_avail *)
		gpa_to_hvm_va((unsigned long)queue->avail);
	queue->used = (struct vring_used *)
		gpa_to_hvm_va((unsigned long)queue->used);

	pr_debug("find virt_queue %d 0x%p 0x%p 0x%p\n", queue->num,
			queue->desc, queue->avail, queue->used);

	queue->last_avail_idx = 0;
	queue->avail_idx = 0;
	queue->last_used_idx = 0;
	queue->signalled_used = 0;
	queue->signalled_used_valid = 0;

	if (dev->ops && dev->ops->vq_init)
		return dev->ops->vq_init(dev, queue);

	return 0;
}

int virtio_handle_event(struct virtio_device *dev)
{
	int ret;
	void *iomem = dev->vdev->iomem;
	uint32_t event_type, arg;

	event_type = ioread32(iomem + VIRTIO_MMIO_EVENT);
	iowrite32(iomem + VIRTIO_MMIO_EVENT, 0);

	switch (event_type) {
	case VIRTIO_EVENT_STATUS_CHANGE:
		arg = ioread32(iomem + VIRTIO_MMIO_STATUS);
		ret = virtio_status_event(dev, arg);
		break;

	case VIRTIO_EVENT_QUEUE_READY:
		arg = ioread32(iomem + VIRTIO_MMIO_QUEUE_NOTIFY);
		ret = virtio_queue_event(dev, arg);
		break;

	case VIRTIO_EVENT_MMIO:
		arg = ioread32(iomem + VIRTIO_MMIO_EVENT_ARG);
		ret = virtio_mmio_event(dev, arg);
		break;

	case VIRTIO_EVENT_BUFFER_READY:
		arg = ioread32(iomem + VIRTIO_MMIO_QUEUE_SEL);
		ret = virtio_buffer_event(dev, arg);
		break;

	default:
		pr_err("unsupport virtio event %d\n", event_type);
		break;
	}

	virtio_hvm_ack(dev);

	return ret;
}
