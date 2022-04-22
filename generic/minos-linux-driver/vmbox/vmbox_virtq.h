#ifndef __LINUX_VMBOX_VRING_H__
#define __LINUX_VMBOX_VRING_H__

#include <uapi/linux/virtio_ring.h>

struct vmbox_device;

#define VMBOX_VIRTQ_STOPPED	0x0
#define VMBOX_VIRTQ_STARTED	0x1

#define VMBOX_VIRTQ_OUT		0x0
#define VMBOX_VIRTQ_IN		0x1
#define VMBOX_VIRTQ_BOTH	0x2

#define VMBOX_VIRTQ_MAX_DB	64
#define VMBOX_VRING_ALIGN	8

struct vmbox_vring_buf {
	int id;
	void *addr;
	size_t size;
};

struct vmbox_virtqueue {
	int num;
	int vring_size;
	int index;
	bool broken;
	void *vring_buf;
	void *pdata;
	int direction;		/* in or out or both */
	int status;		/* the status of this virtqueue */

	int free_head;
	int num_free;
	int num_added;

	/*
	 * member to recoder the data buffer each virtq will
	 * have max 64 data buffer
	 * db_lock may not needed, TO BE FIXED
	 */
	int mask_grp;
	uint8_t mask_tbl[8];
	int free_db;
	spinlock_t db_lock;

	struct vring_desc *desc;
	struct vring_avail *avail;
	struct vring_used *used;

	bool event;

	uint16_t last_avail_idx;
	uint16_t avail_idx;
	uint16_t avail_flags;

	uint16_t last_used_idx;
	uint16_t used_flags;
	uint16_t signalled_used;
	uint16_t signalled_used_valid;

	int (*callback)(struct vmbox_virtqueue *);
};

static inline unsigned int
vmbox_virtq_vring_desc_size(unsigned int qsz, unsigned long align)
{
	int desc_size;

	desc_size = sizeof(struct vring_desc) * qsz + (align - 1);
	desc_size &= ~(align - 1);

	return desc_size;

}

static inline unsigned int
vmbox_virtq_vring_avail_size(unsigned int qsz, unsigned long align)
{
	int  avail_size;

	avail_size = sizeof(uint16_t) * (3 + qsz) + (align - 1);
	avail_size &= ~(align - 1);

	return avail_size;

}

static inline unsigned int
vmbox_virtq_vring_used_size(unsigned int qsz, unsigned long align)
{
	int used_size;

	used_size = sizeof(uint16_t) * 2 + sizeof(struct vring_used_elem) *
		(qsz + 1) + (align - 1);
	used_size &= ~(align - 1);

	return used_size;

}

static inline unsigned int
vmbox_virtq_vring_size(unsigned int qsz, unsigned long align)
{
	return vmbox_virtq_vring_desc_size(qsz, align) +
		vmbox_virtq_vring_avail_size(qsz, align) +
		vmbox_virtq_vring_used_size(qsz, align);
}

static int inline vmbox_virtq_more_avail(struct vmbox_virtqueue *vq)
{
	return vq->avail->idx != vq->last_avail_idx;
}

static inline int vmbox_virtq_more_used(struct vmbox_virtqueue *vq)
{
	return vq->last_used_idx != vq->used->idx;
}

static inline void vmbox_virtq_startup(struct vmbox_virtqueue *vq)
{
	vq->status = VMBOX_VIRTQ_STARTED;
}

static inline void vmbox_virtq_shutdown(struct vmbox_virtqueue *vq)
{
	vq->status = VMBOX_VIRTQ_STOPPED;
}

void vmbox_virtq_startup(struct vmbox_virtqueue *vq);
void vmbox_virtq_shutdown(struct vmbox_virtqueue *vq);

int vmbox_virtq_add_outbuf(struct vmbox_virtqueue *vq,
			struct vmbox_vring_buf *buf, int outs);
int vmbox_virtq_add_inbuf(struct vmbox_virtqueue *vq,
			struct vmbox_vring_buf *buf, int ins);

void vmbox_virtq_discard_desc(struct vmbox_virtqueue *vq, int n);

int vmbox_virtq_add_used(struct vmbox_virtqueue *vq,
		unsigned int head, uint32_t len);

int vmbox_virtq_get_used_bufs(struct vmbox_virtqueue *vq,
		struct vmbox_vring_buf *buf, int cnt, int *result);
int vmbox_virtq_get_used_buf(struct vmbox_virtqueue *vq,
		struct vmbox_vring_buf *buf);

int vmbox_virtq_get_avail_bufs(struct vmbox_virtqueue *vq,
		struct vmbox_vring_buf *buf, int cnt, int *result);
int vmbox_virtq_get_avail_buf(struct vmbox_virtqueue *vq,
		struct vmbox_vring_buf *buf);

void vmbox_virtq_detach_buf(struct vmbox_virtqueue *vq,
		struct vmbox_vring_buf *buf, int cnt);

bool vmbox_virtq_notify_avail(struct vmbox_virtqueue *vq);
bool vmbox_virtq_notify_used(struct vmbox_virtqueue *vq);

int virtq_add_used_n(struct vmbox_virtqueue *vq,
		struct vring_used_elem *heads,
		unsigned int count);

void vmbox_virtq_add_used_and_signal(struct vmbox_virtqueue *vq,
		unsigned int head, int len);

void vmbox_virtq_add_used_and_signal_n(struct vmbox_virtqueue *vq,
				struct vring_used_elem *heads,
				unsigned int count);

void vmbox_virtq_consume_descs(struct vmbox_virtqueue *vq);

int vmbox_virtq_get_vring_buf(struct vmbox_virtqueue *vq,
		struct vmbox_vring_buf *buf, int cnt);

void vmbox_virtq_init(struct vmbox_device *vdev,
		struct vmbox_virtqueue *vq, int index);

#endif
