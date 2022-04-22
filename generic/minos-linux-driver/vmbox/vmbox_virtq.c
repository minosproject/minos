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

#include <linux/virtio.h>
#include <linux/virtio_ring.h>
#include <linux/virtio_config.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/hrtimer.h>
#include "vmbox_bus.h"

#define vmbox_virtq_wmb()	wmb()
#define vmbox_virtq_rmb()	rmb()
#define vmbox_virtq_mb()	mb()

#define vmbox_virtq_used_event(vq) \
	(uint16_t *)&vq->avail->ring[vq->num]
#define vmbox_virtq_avail_event(vq) \
	(uint16_t *)&vq->used->ring[vq->num]

uint8_t const ffs_table[256] = {
	0u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, /* 0x00 to 0x0F */
	4u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, /* 0x10 to 0x1F */
	5u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, /* 0x20 to 0x2F */
	4u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, /* 0x30 to 0x3F */
	6u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, /* 0x40 to 0x4F */
	4u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, /* 0x50 to 0x5F */
	5u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, /* 0x60 to 0x6F */
	4u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, /* 0x70 to 0x7F */
	7u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, /* 0x80 to 0x8F */
	4u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, /* 0x90 to 0x9F */
	5u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, /* 0xA0 to 0xAF */
	4u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, /* 0xB0 to 0xBF */
	6u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, /* 0xC0 to 0xCF */
	4u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, /* 0xD0 to 0xDF */
	5u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, /* 0xE0 to 0xEF */
	4u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u  /* 0xF0 to 0xFF */
};

static inline int
__vmbox_get_vring_buf(struct vmbox_virtqueue *vq, int *x, int *y)
{
	uint8_t *tbl = vq->mask_tbl;
	int grp = vq->mask_grp;

	*y = ffs_table[grp];
	*x = ffs_table[tbl[*y]];

	return (*y << 3) + *x;
}

static inline void
__vmbox_mask_vring_buf(struct vmbox_virtqueue *vq, int x, int y)
{
	vq->mask_grp |= (1 << y);
	vq->mask_tbl[y] |= (1 << x);
}

static inline void
__vmbox_unmask_vring_buf(struct vmbox_virtqueue *vq, int x, int y)
{
	vq->mask_tbl[y] &= ~(1 << x);
	if (vq->mask_tbl[y] == 0)
		vq->mask_grp &= ~(1 << y);
}

static inline void
vmbox_mask_vring_buf(struct vmbox_virtqueue *vq, int x, int y)
{
	unsigned long flags;

	spin_lock_irqsave(&vq->db_lock, flags);
	__vmbox_mask_vring_buf(vq, x, y);
	spin_unlock_irqrestore(&vq->db_lock, flags);
}

static inline void
vmbox_unmask_vring_buf(struct vmbox_virtqueue *vq, int x, int y)
{
	unsigned long flags;

	spin_lock_irqsave(&vq->db_lock, flags);
	__vmbox_unmask_vring_buf(vq, x, y);
	spin_unlock_irqrestore(&vq->db_lock, flags);
}

int vmbox_virtq_get_vring_buf(struct vmbox_virtqueue *vq,
		struct vmbox_vring_buf *buf, int cnt)
{
	int i, id, x, y;
	unsigned long flags;
	struct vmbox_vring_buf *vbuf = buf;

	spin_lock_irqsave(&vq->db_lock, flags);

	if (vq->free_db < cnt) {
		spin_unlock_irqrestore(&vq->db_lock, flags);
		return -ENOSPC;
	}

	for (i = 0; i < cnt; i++) {
		id = __vmbox_get_vring_buf(vq, &x, &y);
		vbuf->id = id;
		vbuf->addr = vq->vring_buf + vq->vring_size * id;
		vbuf->size = vq->vring_size;
		__vmbox_unmask_vring_buf(vq, id % 8, id / 8);
	}

	vq->free_db -= cnt;
	spin_unlock_irqrestore(&vq->db_lock, flags);

	return 0;
}

void vmbox_virtq_detach_buf(struct vmbox_virtqueue *vq,
		struct vmbox_vring_buf *buf, int cnt)
{
	int i, x, y;
	unsigned long flags;
	struct vmbox_vring_buf *vbuf = buf;

	spin_lock_irqsave(&vq->db_lock, flags);

	for (i = 0; i < cnt; i++) {
		if (vbuf->id >= vq->num) {
			pr_err("Invaild data buffer id %d\n", vbuf->id);
			continue;
		}

		y = vbuf->id / 8;
		x = vbuf->id % 8;
		__vmbox_mask_vring_buf(vq, x, y);
	}

	vq->free_db += cnt;

	spin_unlock_irqrestore(&vq->db_lock, flags);
}

static inline int vmbox_next_desc(struct vring_desc *desc)
{
	return (!(desc->flags & VRING_DESC_F_NEXT)) ? -1 : desc->next;
}

static int inline vmbox_virtq_need_event(uint16_t event_idx,
		uint16_t new_idx, uint16_t old_idx)
{
	return (uint16_t)(new_idx - event_idx - 1) <
				(uint16_t)(new_idx - old_idx);
}

static inline void vmbox_translate_desc(struct vmbox_virtqueue *vq,
		struct vring_desc *desc, struct vmbox_vring_buf *buf)
{
	buf->addr = vq->vring_buf + desc->addr * vq->vring_size;
	buf->size = desc->len;
	buf->id = desc->addr;
}

static inline bool vmbox_virtq_notify(struct vmbox_virtqueue *vq)
{
	if (unlikely(vq->broken))
		return false;

	vmbox_device_vring_event(vq->pdata);

	return true;
}

static int vmbox_virtq_need_notify_avail(struct vmbox_virtqueue *vq)
{
	uint16_t new, old;
	bool need_kick;

	vmbox_virtq_mb();

	old = vq->last_avail_idx - vq->num_added;
	new = vq->avail_idx;
	vq->num_added = 0;

	/*
	 * host used the last member of used->ring to record the
	 * index of the desc it is handle in the avail ring
	 */
	if (vq->event)
		need_kick = vmbox_virtq_need_event(*vmbox_virtq_avail_event(vq), new, old);
	else
		need_kick = !(vq->used->flags & VRING_USED_F_NO_NOTIFY);

	return need_kick;
}

bool vmbox_virtq_notify_avail(struct vmbox_virtqueue *vq)
{
	if (vmbox_virtq_need_notify_avail(vq))
		return vmbox_virtq_notify(vq);
	return true;
}

static inline int vmbox_virtq_add_buf(struct vmbox_virtqueue *vq,
		struct vmbox_vring_buf *buf, int outs, int ins)
{
	int head;
	int cnt = outs + ins;
	struct vring_desc *desc;
	unsigned int i, n, avail, prev = 0;
	struct vmbox_vring_buf *vbuf = buf;

	if (unlikely(vq->broken))
		return -EIO;

	if (cnt == 0)
		return -EINVAL;

	if (vq->num_free < cnt) {
		vmbox_virtq_notify(vq);
		return -ENOSPC;
	}

	i = head = vq->free_head;
	desc = vq->desc;

	for (n = 0; n < outs; n++) {
		desc[i].flags = VRING_DESC_F_NEXT;
		desc[i].len = vbuf->size;
		desc[i].addr = vbuf->id;
		prev = i;
		i = desc[i].next;
		vbuf++;
	}

	for (; n < cnt; n++) {
		desc[i].flags = VRING_DESC_F_NEXT | VRING_DESC_F_WRITE;
		desc[i].len = vbuf->size;
		desc[i].addr = vbuf->id;
		prev = i;
		i = desc[i].next;
		vbuf++;
	}

	desc[prev].flags &= ~VRING_DESC_F_NEXT;

	/* update the free number and the new head */
	vq->num_free -= cnt;
	vq->free_head = i;

	avail = vq->last_avail_idx & (vq->num - 1);
	vq->avail->ring[avail] = head;

	vmbox_virtq_wmb();
	vq->last_avail_idx++;
	vq->avail->idx = vq->last_avail_idx;
	vq->num_added++;

	return head;
}

int vmbox_virtq_add_inbuf(struct vmbox_virtqueue *vq,
			struct vmbox_vring_buf *buf, int ins)
{
	return vmbox_virtq_add_buf(vq, buf, 0, ins);
}

int vmbox_virtq_add_outbuf(struct vmbox_virtqueue *vq,
			struct vmbox_vring_buf *buf, int outs)
{
	return vmbox_virtq_add_buf(vq, buf, outs, 0);
}

/*
 * vmbox_virtq_get_used_bufs used to get data buffers from used
 * ring
 */
int vmbox_virtq_get_used_bufs(struct vmbox_virtqueue *vq,
		struct vmbox_vring_buf *buf, int cnt, int *result)
{
	unsigned int i, m, num = 0;
	uint16_t last_used;
	struct vmbox_vring_buf *vbuf = buf;

	if (unlikely(vq->broken))
		return -EIO;

	if (!vmbox_virtq_more_used(vq))
		return -ENOSPC;

	if (result)
		*result = 0;

	vmbox_virtq_rmb();

	last_used = vq->last_used_idx & (vq->num - 1);
	m = i = vq->used->ring[last_used].id;

	do {
		num++;
		vmbox_translate_desc(vq, &vq->desc[m], vbuf);
		if (!(vq->desc[m].flags & VRING_DESC_F_NEXT))
			break;

		m = vq->desc[m].next;
		vq->num_free++;
		vbuf++;

		if (num == cnt) {
			pr_warn("no more used vring buffer\n");
			return -ENOSPC;
		}
	} while (1);

	/*
	 * update the free_head of the virtqueue and put
	 * the old free_head to the next field
	 */
	vq->desc[m].next = vq->free_head;
	vq->free_head = i;

	/* Plus final descriptor of the ring */
	vq->num_free++;
	vq->last_used_idx++;

	if (result)
		*result = num;

	/* If we expect an interrupt for the next entry, tell host
	 * by writing event index and flush out the write before
	 * the read in the next get_buf call. */
	if (!(vq->avail_flags & VRING_AVAIL_F_NO_INTERRUPT)) {
		*vmbox_virtq_used_event(vq) = vq->last_used_idx;
		vmbox_virtq_mb();
	}

	return i;
}

int vmbox_virtq_get_used_buf(struct vmbox_virtqueue *vq,
		struct vmbox_vring_buf *buf)
{
	return vmbox_virtq_get_used_bufs(vq, buf, 1, NULL);
}

/*
 * vmbox_virtq_get_avail_bufs used to get some data buffer
 * from the avail ring
 */
int vmbox_virtq_get_avail_bufs(struct vmbox_virtqueue *vq,
		struct vmbox_vring_buf *buf, int cnt, int *result)
{
	int ret = 0;
	struct vring_desc *desc;
	unsigned int i, head;
	uint32_t last_avail_idx;
	uint16_t avail_idx;
	uint32_t count;

	if (result)
		*result = 0;

	vmbox_virtq_rmb();

	last_avail_idx = vq->last_avail_idx;
	avail_idx = vq->avail->idx;
	vq->avail_idx = avail_idx;

	/* to avoid uint16_t overflow */
	count = (uint16_t)((uint32_t)avail_idx - last_avail_idx);
	if (count == 0)
		return -ENOSPC;

	if (count > vq->num) {
		pr_err("avail ring out of range %d %d\n",
				avail_idx, last_avail_idx);
		return -EINVAL;
	}

	i = head = vq->avail->ring[last_avail_idx & (vq->num - 1)];
	if (head >= vq->num) {
		pr_err("avail ring idx out of range\n");
		return -EINVAL;
	}

	do {
		if (ret >= cnt) {
			pr_err("avail count bigger than buf count %d %d %d\n",
					avail_idx, last_avail_idx, cnt);
			return -EINVAL;
		}

		if (i >= vq->num) {
			pr_err("desc index %d > %d head = %d\n",
					i, vq->num, head);
			return -EINVAL;
		}

		desc = &vq->desc[i];
		if (desc->flags & VRING_DESC_F_INDIRECT) {
			pr_err("Vmbox virtq desc do not support INDRIECT feature\n");
			return -EINVAL;
		}

		vmbox_translate_desc(vq, desc, &buf[ret]);
		ret++;
	} while ((i = vmbox_next_desc(desc)) != -1);

	if (result)
		*result = ret;

	vq->last_avail_idx++;

	/*
	 * If we expect an interrupt for the next entry, tell the
	 * guest or productor by writing event index and flush out
	 */
	if (!(vq->used_flags & VRING_AVAIL_F_NO_INTERRUPT)) {
		*vmbox_virtq_avail_event(vq) = vq->last_avail_idx;
		vmbox_virtq_mb();
	}

	return head;
}

int vmbox_virtq_get_avail_buf(struct vmbox_virtqueue *vq,
		struct vmbox_vring_buf *buf)
{
	return vmbox_virtq_get_avail_bufs(vq, buf, 1, NULL);
}

void vmbox_virtq_discard_desc(struct vmbox_virtqueue *vq, int n)
{
	vq->last_avail_idx -= n;
	vmbox_virtq_mb();
}

static int __vmbox_virtq_add_used_n(struct vmbox_virtqueue *vq,
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
	} else {
		memcpy(used, heads, count * sizeof(*used));
	}

	old = vq->last_used_idx;
	vq->last_used_idx += count;
	new = vq->last_used_idx;

	if (((uint16_t)(new - vq->signalled_used)) < ((uint16_t)(new - old)))
		vq->signalled_used_valid = 0;

	return 0;
}

int vmbox_virtq_add_used_n(struct vmbox_virtqueue *vq,
		struct vring_used_elem *heads,
		unsigned int count)
{
	int start, n, r;

	start = vq->last_used_idx & (vq->num - 1);
	n = vq->num - start;
	if (n < count) {
		r = __vmbox_virtq_add_used_n(vq, heads, n);
		if (r < 0)
			return r;
		heads += n;
		count -= n;
	}

	r = __vmbox_virtq_add_used_n(vq, heads, count);

	vq->used->idx = vq->last_used_idx;

	return r;
}

int vmbox_virtq_add_used(struct vmbox_virtqueue *vq,
		unsigned int head, uint32_t len)
{
	struct vring_used_elem heads = {
		.id = head,
		.len = len,
	};

	return vmbox_virtq_add_used_n(vq, &heads, 1);
}

static int vmbox_virtq_need_notify_used(struct vmbox_virtqueue *vq)
{
	uint16_t old, new;
	uint16_t event;
	int notify;

	old = vq->signalled_used;
	notify = vq->signalled_used_valid;
	new = vq->signalled_used = vq->last_used_idx;
	vq->signalled_used_valid = 1;

	if (!notify)
		return 1;

	event = *vmbox_virtq_used_event(vq);

	return vmbox_virtq_need_event(event, new, old);
}

bool vmbox_virtq_notify_used(struct vmbox_virtqueue *vq)
{
	if (vmbox_virtq_need_notify_used(vq))
		return vmbox_virtq_notify(vq);
	return true;
}

void vmbox_virtq_add_used_and_signal(struct vmbox_virtqueue *vq,
		unsigned int head, int len)
{
	vmbox_virtq_add_used(vq, head, len);
	vmbox_virtq_notify_used(vq);
}

void vmbox_virtq_add_used_and_signal_n(struct vmbox_virtqueue *vq,
				struct vring_used_elem *heads,
				unsigned int count)
{
	vmbox_virtq_add_used_n(vq, heads, count);
	vmbox_virtq_notify_used(vq);
}

void vmbox_virtq_consume_descs(struct vmbox_virtqueue *vq)
{
	int index;
	struct vmbox_vring_buf buf;

	pr_info("%s\n", __func__);

	if (vq->direction == VMBOX_VIRTQ_OUT)
		return;

	while (1) {
		index = vmbox_virtq_get_avail_buf(vq, &buf);
		if (index < 0)
			break;

		vmbox_virtq_add_used(vq, index, 0);
	}

	vmbox_virtq_notify_used(vq);
}

static inline void *vmbox_vq_vring_base(struct vmbox_device *vdev, int index)
{
	size_t size = vmbox_virtq_vring_size(vdev->vring_num, VMBOX_VRING_ALIGN);

	return vdev->data_base + index * size;
}

static inline void *vmbox_vq_buf_base(struct vmbox_device *vdev, int index)
{
	void *base;
	size_t total_size;
	size_t size = vmbox_virtq_vring_size(vdev->vring_num,
			VMBOX_VRING_ALIGN);

	total_size = size * vdev->nr_vqs;
	base = vdev->data_base + total_size;
	base += (vdev->vring_size * vdev->vring_num) * index;

	return base;
}

void vmbox_virtq_init(struct vmbox_device *vdev,
		struct vmbox_virtqueue *vq, int index)
{
	int i;
	void *vring_base;
	int num = vdev->vring_num;

	vring_base = vmbox_vq_vring_base(vdev, index);
	vq->vring_buf = vmbox_vq_buf_base(vdev, index);

	vq->num = num;
	spin_lock_init(&vq->db_lock);
	vq->pdata = vdev;
	vq->index = index;
	vq->num = num;
	vq->vring_size = vdev->vring_size;
	vq->free_head = 0;
	vq->free_db = num;
	vq->num_free = num;
	vq->desc = vring_base;
	vq->avail = vring_base +
		vmbox_virtq_vring_desc_size(num, VMBOX_VRING_ALIGN);
	vq->used = vring_base +
		vmbox_virtq_vring_desc_size(num, VMBOX_VRING_ALIGN) +
		vmbox_virtq_vring_avail_size(num, VMBOX_VRING_ALIGN);
	/*
	 * set up the TX vq
	 */
	for (i = 0; i < num - 1; i++)
		vq->desc[i].next = __cpu_to_virtio16(1, i + 1);

	/* init the mask table set unused vring_buf bit to 1 */
	for (i = 0; i < num; i++)
		__vmbox_mask_vring_buf(vq, i % 8, i / 8);
}
