/*
 * BSD 3-Clause License
 *
 * Copyright (C) 2018 Min Le (lemin9538@gmail.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <mvm.h>
#include <virtio.h>
#include <virtio_mmio.h>
#include <io.h>
#include <barrier.h>

static void *virtio_guest_iobase;
static void *virtio_host_iobase;
static size_t virtio_iomem_size;
static size_t virtio_iomem_free;

static int hv_create_virtio_device(struct vm *vm,
		void **gbase, void **hbase)
{
	int ret;
	void *__gbase, *__hbase;

	if (virtio_iomem_free < VIRTIO_DEVICE_IOMEM_SIZE)
		return -ENOMEM;

	__gbase = virtio_guest_iobase + (virtio_iomem_size - virtio_iomem_free);
	__hbase = virtio_host_iobase + (virtio_iomem_size - virtio_iomem_free);
	virtio_iomem_free -= VIRTIO_DEVICE_IOMEM_SIZE;

	ret = ioctl(vm->vm_fd, IOCTL_CREATE_VIRTIO_DEVICE,
			(unsigned long)__gbase);
	if (ret) {
		pr_err("create virtio device failed %d\n", ret);
		return ret;
	}

	*gbase = __gbase;
	*hbase = __hbase;

	return 0;
}

static int hv_virtio_mmio_deinit(struct vm *vm)
{
	return ioctl(vm->vm_fd, IOCTL_VIRTIO_MMIO_DEINIT, NULL);
}

static int hv_virtio_mmio_init(struct vm *vm,
		size_t size, void **gbase, void **hbase)
{
	int ret = 0;
	void *map_base;
	uint64_t args[2] = {size, 0};

	ret = ioctl(vm->vm_fd, IOCTL_VIRTIO_MMIO_INIT, args);
	if (ret || !args[0] || !args[1]) {
		pr_err("virtio mmio init failed in hypervisor\n");
		return ret;
	}

	map_base = vdev_map_iomem((void *)((unsigned long)args[1]), size);
	if (map_base == (void *)-1) {
		hv_virtio_mmio_deinit(vm);
		return -ENOMEM;
	}


	*gbase = (void *)(unsigned long)args[0];
	*hbase = map_base;

	return 0;
}

int virtio_mmio_init(struct vm *vm, int nr_devs)
{
	int ret = 0;
	size_t size;
	void *gbase = NULL, *hbase = NULL;

	/*
	 * each virtio device will have 0x400 iomem
	 * space for communicated between guest and host
	 *
	 * and the total virtio mem space must PAGE_ALIGN
	 */
	size = nr_devs * VIRTIO_DEVICE_IOMEM_SIZE;
	size = BALIGN(size, PAGE_SIZE);

	ret = hv_virtio_mmio_init(vm, size, &gbase, &hbase);
	if (ret || !gbase || !hbase)
		return ret;

	virtio_guest_iobase = gbase;
	virtio_host_iobase = hbase;
	virtio_iomem_size = size;
	virtio_iomem_free = size;

	pr_info("virtio-mmio : 0x%p 0x%p 0x%lx\n", gbase, hbase, size);

	return 0;
}

int virtio_mmio_deinit(struct vm *vm)
{
	vdev_unmap_iomem(virtio_host_iobase, virtio_iomem_size);

	return hv_virtio_mmio_deinit(vm);
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
		struct iovec *iov, int iov_size, unsigned int *in,
		unsigned int *out)
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

		translate_desc(vd, index, iov, iov_size);
		if (desc->flags & VRING_DESC_F_WRITE)
			*in += 1;
		else
			*out += 1;

		index++;
		if (index >= iov_size) {
			pr_err("%d out of ivo size\n", index);
			return -ENOMEM;
		}

		next = next_desc(vd);
		if (next == -1)
			break;
		else if (next >= nr_in) {
			pr_err("out of indirect desc range\n");
			return -EINVAL;
		}
	}

	/* return how many iovs get in the desc */
	return (index - old_index);
}

static void virtq_update_used_flags(struct virt_queue *vq)
{
	vq->used->flags = vq->used_flags;
	mb();
}

static void virtq_update_avail_event(struct virt_queue *vq, uint16_t event)
{
	*virtq_avail_event(vq) = event;
	mb();
}

int virtq_enable_notify(struct virt_queue *vq)
{
	uint16_t avail_idx;

	if (!(vq->used_flags & VRING_USED_F_NO_NOTIFY))
		return 0;

	vq->used_flags &= ~VRING_USED_F_NO_NOTIFY;
	if (!virtq_has_feature(vq, VIRTIO_RING_F_EVENT_IDX))
		virtq_update_used_flags(vq);
	else
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
	uint32_t last_avail_idx;
	uint16_t avail_idx;
	uint32_t count;
	int iov_index = 0, ret;

	rmb();

	last_avail_idx = vq->last_avail_idx;
	avail_idx = vq->avail->idx;
	vq->avail_idx = avail_idx;

	/* to avoid uint16_t overflow */
	count = (uint16_t)((uint32_t)avail_idx - last_avail_idx);
	if (count == 0)
		return vq->num;

	if (count > vq->num) {
		pr_err("avail ring out of range %d %d\n",
				avail_idx, last_avail_idx);
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
			ret = get_indirect_buf(desc, iov_index, iov,
					iov_size, in_num, out_num);
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
	wmb();
}

static int __virtq_add_used_n(struct virt_queue *vq,
			struct vring_used_elem *heads,
			unsigned int count)
{
	struct vring_used_elem *used;
	uint16_t old, new;
	int start;

	start = vq->last_used_idx & (vq->num - 1);
	used = vq->used->ring + start;

	if (count == 1) {
		used->id = heads[0].id;
		used->len = heads[0].len;
	} else
		memcpy(used, heads, count * sizeof(*used));

	wmb();

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

	start = vq->last_used_idx & (vq->num - 1);
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

static int virtq_need_notify(struct virt_queue *vq)
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

void virtq_notify(struct virt_queue *vq)
{
	if (virtq_need_notify(vq))
		virtio_send_irq(vq->dev, VIRTIO_MMIO_INT_VRING);
}

void virtq_add_used_and_signal(struct virt_queue *vq,
		unsigned int head, int len)
{
	virtq_add_used(vq, head, len);
	virtq_notify(vq);
}

void virtq_add_used_and_signal_n(struct virt_queue *vq,
				struct vring_used_elem *heads,
				unsigned int count)
{
	virtq_add_used_n(vq, heads, count);
	virtq_notify(vq);
}

static int __virtio_vdev_init(struct vdev *vdev,
		void *gbase, void *hbase, int type, int rs)
{
	if (!gbase || !hbase)
		return -EINVAL;

	vdev->dev_type = VDEV_TYPE_VIRTIO;
	vdev->iomem = hbase;
	vdev->guest_iomem = gbase;
	vdev->gvm_irq = ioread32(hbase + VIRTIO_MMIO_GVM_IRQ);

	pr_debug("vdev : irq-%d hpa-0x%p gva-0x%p\n", vdev->gvm_irq,
			vdev->iomem, vdev->guest_iomem);

	if (rs > VIRTQUEUE_MAX_SIZE)
		rs = VIRTQUEUE_MAX_SIZE;

	iowrite32(hbase + VIRTIO_MMIO_MAGIC_VALUE, VIRTIO_MMIO_MAGIG);
	iowrite32(hbase + VIRTIO_MMIO_VERSION, VIRTIO_VERSION);
	iowrite32(hbase + VIRTIO_MMIO_VENDOR_ID, VIRTIO_VENDER_ID);
	iowrite32(hbase + VIRTIO_MMIO_DEVICE_ID, type);
	iowrite32(hbase + VIRTIO_MMIO_QUEUE_NUM_MAX, rs);

	return 0;
}

static void inline virtq_reset(struct virt_queue *vq)
{
	vq->desc = NULL;
	vq->avail = NULL;
	vq->used = NULL;
	vq->last_avail_idx = 0;
	vq->avail_idx = 0;
	vq->last_used_idx = 0;
	vq->used_flags = 0;
	vq->signalled_used = 0;
	vq->signalled_used_valid = 0;
	vq->ready = 0;
}

int virtio_device_reset(struct virtio_device *dev)
{
	int i;

	for (i = 0; i < dev->nr_vq; i++) {
		if (dev->ops && dev->ops->vq_reset)
			dev->ops->vq_reset(&dev->vqs[i]);

		virtq_reset(&dev->vqs[i]);
	}

	return 0;
}

void virtio_device_deinit(struct virtio_device *virt_dev)
{
	int i;
	struct virt_queue *vq;

	for (i = 0; i < virt_dev->nr_vq; i++) {
		vq = &virt_dev->vqs[i];
		if (virt_dev->ops && virt_dev->ops->vq_deinit)
			virt_dev->ops->vq_deinit(vq);

		if (vq->iovec)
			free(vq->iovec);
	}

	if (virt_dev->vqs)
		free(virt_dev->vqs);
}

int virtio_device_init(struct virtio_device *virt_dev, struct vdev *vdev,
		int type, int queue_nr, int rs, int iov_size)
{
	void *gbase, *hbase;
	int ret, i;
	struct virt_queue *vq;

	if (!virt_dev || !vdev)
		return -EINVAL;

	if ((type == 0) || (type > 18) ||
			((type > 9) && (type < 18))) {
		pr_err("unsupport virtio device type %d\n", type);
		return -EINVAL;
	}

	ret = hv_create_virtio_device(vdev->vm, &gbase, &hbase);
	if (ret)
		return -ENOMEM;

	if (rs > VIRTQUEUE_MAX_SIZE)
		rs = VIRTQUEUE_MAX_SIZE;

	ret = __virtio_vdev_init(vdev, gbase, hbase, type, rs);
	if (ret) {
		pr_err("failed to init virtio device\n");
		return ret;
	}

	vdev->iomem_size = VIRTIO_DEVICE_IOMEM_SIZE;
	virt_dev->vdev = vdev;
	virt_dev->config = vdev->iomem + VIRTIO_MMIO_CONFIG;

	/* alloc memory for virtio queue */
	virt_dev->vqs = malloc(sizeof(struct virt_queue) * queue_nr);
	if (!virt_dev->vqs) {
		ret = -ENOMEM;
		goto release_virtio_dev;
	}

	virt_dev->nr_vq = queue_nr;
	memset(virt_dev->vqs, 0, sizeof(struct virt_queue) * queue_nr);

	if (iov_size > VIRTQUEUE_MAX_SIZE)
		iov_size = VIRTQUEUE_MAX_SIZE;

	/* alloc the iovec */
	for (i = 0; i < queue_nr; i++) {
		vq = &virt_dev->vqs[i];
		vq->iovec = malloc(sizeof(struct iovec) * iov_size);
		if (!vq->iovec) {
			pr_err("failed to get memory for iovec %d\n", i);
			goto release_virtio_dev;
		}

		vq->iovec_size = iov_size;
	}

	return 0;

release_virtio_dev:
	virtio_device_deinit(virt_dev);
	return ret;
}

static int virtio_status_event(struct virtio_device *dev, uint32_t arg)
{
	void *iomem = dev->vdev->iomem;

	switch (arg) {
	case VIRTIO_DEV_NEEDS_RESET:
		break;

	case VIRTIO_DEV_STATUS_OK:
		break;

	case VIRTIO_DEV_STATUS_ACK:
		break;

	case VIRTIO_DEV_STATUS_DRIVER:
		break;

	case VIRTIO_DEV_STATUS_FEATURES_OK:
		dev->acked_features = u32_to_u64(
			ioread32(iomem + VIRTIO_MMIO_DRIVER_FEATURE1),
			ioread32(iomem + VIRTIO_MMIO_DRIVER_FEATURE0));

		if (dev->ops && dev->ops->neg_features)
			dev->ops->neg_features(dev);
		break;

	case VIRTIO_DEV_STATUS_FAILED:
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

	if (queue->callback)
		queue->callback(queue);
	else
		pr_err("no callback for this virt queue\n");

	return 0;
}

static int virtio_buffer_event(struct virtio_device *dev, uint32_t arg)
{
	struct virt_queue *vq;
	void *iomem = dev->vdev->iomem;
	uint32_t high, low;

	if (arg >= dev->nr_vq) {
		pr_err("invaild virt queue index %d %s\n",
				arg, dev->vdev->name);
		return -EINVAL;
	}

	vq = &dev->vqs[arg];
	vq->vq_index = arg;
	vq->dev = dev;
	vq->num = ioread32(iomem + VIRTIO_MMIO_QUEUE_NUM);

	high = ioread32(iomem + VIRTIO_MMIO_QUEUE_DESC_HIGH);
	low = ioread32(iomem + VIRTIO_MMIO_QUEUE_DESC_LOW);
	vq->desc = (struct vring_desc *)u32_to_u64(high, low);

	high = ioread32(iomem + VIRTIO_MMIO_QUEUE_AVAIL_HIGH);
	low = ioread32(iomem + VIRTIO_MMIO_QUEUE_AVAIL_LOW);
	vq->avail = (struct vring_avail *)u32_to_u64(high, low);

	high = ioread32(iomem + VIRTIO_MMIO_QUEUE_USED_HIGH);
	low = ioread32(iomem + VIRTIO_MMIO_QUEUE_USED_LOW);
	vq->used = (struct vring_used *)u32_to_u64(high, low);

	vq->desc = (struct vring_desc *)
		gpa_to_hvm_va((unsigned long)vq->desc);
	vq->avail = (struct vring_avail *)
		gpa_to_hvm_va((unsigned long)vq->avail);
	vq->used = (struct vring_used *)
		gpa_to_hvm_va((unsigned long)vq->used);

	vq->last_avail_idx = 0;
	vq->avail_idx = 0;
	vq->last_used_idx = 0;
	vq->signalled_used = 0;
	vq->signalled_used_valid = 0;
	vq->ready = 1;

	if (dev->ops && dev->ops->vq_init)
		dev->ops->vq_init(vq);

	return 0;
}

static int virtio_mmio_read(struct virtio_device *dev,
		unsigned long addr, unsigned long *value)
{
	pr_err("current guest can read the value directly\n");

	return -EINVAL;
}

static int virtio_mmio_write(struct virtio_device *dev,
		unsigned long addr, unsigned long *value)
{
	int ret = 0;
	unsigned long offset;
	uint32_t arg = (uint32_t)(*value);

	offset = addr - (unsigned long)dev->vdev->guest_iomem;
	switch (offset) {
	case VIRTIO_MMIO_STATUS:
		ret = virtio_status_event(dev, arg);
		break;

	case VIRTIO_MMIO_QUEUE_READY:
		ret = virtio_buffer_event(dev, arg);
		break;

	case VIRTIO_MMIO_QUEUE_NOTIFY:
		ret = virtio_queue_event(dev, arg);
		break;

	default:
		break;
	}

	return ret;
}

int virtio_handle_mmio(struct virtio_device *dev, int write,
		unsigned long addr, unsigned long *value)
{
	if (write == VMTRAP_REASON_WRITE)
		return virtio_mmio_write(dev, addr, value);
	else if (write == VMTRAP_REASON_READ)
		return virtio_mmio_read(dev, addr, value);

	return -EINVAL;
}
