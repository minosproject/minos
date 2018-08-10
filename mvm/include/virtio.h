#ifndef __MVM_VIRT_IO_H_
#define __MVM_VIRT_IO_H_

#include <sys/types.h>
#include <inttypes.h>

#define VRING_DESC_F_NEXT		(0)
#define VRING_DESC_F_WRITE		(1)
#define VRING_DESC_F_INDIRECT		(4)

#define VRING_ALIGN_SIZE		(4096)

#define VRING_AVAIL_F_NO_INTERRUPT	1

#define VIRTIO_RING_F_INDIRECT_DESC	28
#define VIRTIO_RING_F_EVENT_IDX		29

#define VRING_AVAIL_ALIGN_SIZE		2
#define VRING_USED_ALIGN_SIZE		4
#define VRING_DESC_ALIGN_SIZE		16

#define VIRTIO_VENDER_id		(0x8888)

#define	VIRTIO_TYPE_NET			1
#define	VIRTIO_TYPE_BLOCK		2
#define	VIRTIO_TYPE_CONSOLE		3
#define	VIRTIO_TYPE_ENTROPY		4
#define	VIRTIO_TYPE_BALLOON		5
#define	VIRTIO_TYPE_IOMEMORY		6
#define	VIRTIO_TYPE_RPMSG		7
#define	VIRTIO_TYPE_SCSI		8
#define	VIRTIO_TYPE_9P			9
#define	VIRTIO_TYPE_INPUT		18

#define VIRTIO_DEV_STATUS_ACK		(1)
#define VIRTIO_DEV_STATUS_DRIVER	(2)
#define VIRTIO_DEV_STATUS_OK		(4)
#define VIRTIO_DEV_STATUS_FEATURES_OK	(8)
#define VIRTIO_DEV_NEEDS_RESET		(64)
#define VIRTIO_DEV_STATUS_FAILED	(128)

struct vring_used_elem {
	uint32_t id;
	uint32_t len;
} __attribute__((__packed__));

struct vring_used {
	uint16_t flags;
	uint16_t idx;
	struct vring_used_elem ring[];
} __attribute__((__packed__));

struct vring_avail {
	uint16_t flags;
	uint16_t idx;
	uint16_t ring[];
} __attribute__((__packed__));

struct vring_desc {
	uint64_t addr;
	uint32_t len;
	uint16_t flags;
	uint16_t next;
} __attribute__((__packed__));

struct virt_queue {
	unsigned int num;
	struct vring_desc *desc;
	struct vring_avail *avail;
	struct vring_used *used;
};

static int inline virtq_need_event(uint16_t event_idx,
		uint16_t new_idx, uint16_t old_idx)
{
	return (uint16_t)(new_idx - event_idx - 1) <
				(uint16_t)(new_idx - old_idx);
}

static inline uint16_t *virtq_used_event(struct virt_queue *vq)
{
	return (uint16_t *)&vq->avail->ring[vq->num];
}

static inline uint16_t *virtq_avail_event(struct virt_queue *vq)
{
	return (uint16_t *)&vq->used->ring[vq->num];
}

static inline uint32_t vring_size (unsigned int qsz)
{
	return ALIGN((sizeof(struct vring_desc) * qsz) +
		 (sizeof(uint16_t) * (2 + qsz)), VRING_ALIGN_SIZE) +
	       ALIGN(sizeof(struct vring_used_elem) * qsz, VRING_ALIGN_SIZE);
}

#endif
