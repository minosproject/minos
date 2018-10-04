/*-
 * Copyright (c) 2011 NetApp, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <mvm.h>
#include <virtio.h>
#include <block_if.h>
#include <compiler.h>

#define VIRTIO_BLK_RINGSZ	64
#define VIRTIO_BLK_IOVSZ	64

#define VIRTIO_BLK_S_OK		0
#define VIRTIO_BLK_S_IOERR	1
#define	VIRTIO_BLK_S_UNSUPP	2

#define	VIRTIO_BLK_BLK_ID_BYTES	20

/* Capability bits */
#define	VIRTIO_BLK_F_SEG_MAX	(2)	/* Maximum request segments */
#define	VIRTIO_BLK_F_BLK_SIZE	(6)	/* cfg block size valid */
#define	VIRTIO_BLK_F_FLUSH	(9)	/* Cache flush support */
#define	VIRTIO_BLK_F_TOPOLOGY	(10)	/* Optimal I/O alignment */

/* Device can toggle its cache between writeback and writethrough modes */
#define	VIRTIO_BLK_F_CONFIG_WCE	(11)

/*
 * Config space "registers"
 */
struct virtio_blk_config {
	uint64_t capacity;
	uint32_t size_max;
	uint32_t seg_max;
	struct {
		uint16_t cylinders;
		uint8_t heads;
		uint8_t sectors;
	} geometry;
	uint32_t blk_size;
	struct {
		uint8_t physical_block_exp;
		uint8_t alignment_offset;
		uint16_t min_io_size;
		uint32_t opt_io_size;
	} topology;
	uint8_t	writeback;
} __attribute__((packed));

/*
 * Fixed-size block header
 */
struct virtio_blk_hdr {
#define	VBH_OP_READ		0
#define	VBH_OP_WRITE		1
#define	VBH_OP_FLUSH		4
#define	VBH_OP_FLUSH_OUT	5
#define	VBH_OP_IDENT		8
#define	VBH_FLAG_BARRIER	0x80000000	/* OR'ed into type */
	uint32_t type;
	uint32_t ioprio;
	uint64_t sector;
} __attribute__((packed));

struct virtio_blk_ioreq {
	struct blockif_req req;
	struct virtio_blk *blk;
	uint8_t *status;
	uint16_t idx;
};

/*
 * Per-device struct
 */
struct virtio_blk {
	struct virtio_device virtio_dev;
	pthread_mutex_t mtx;
	struct virtio_blk_config *cfg;
	struct blockif_ctxt *bc;
	char ident[VIRTIO_BLK_BLK_ID_BYTES + 1];
	struct virtio_blk_ioreq ios[VIRTIO_BLK_RINGSZ];
	uint8_t original_wce;
};

#define virtio_dev_to_blk(dev) \
	(struct virtio_blk *)container_of(dev, \
			struct virtio_blk, virtio_dev);

#define DBG(...)

static void
virtio_blk_done(struct blockif_req *br, int err)
{
	struct virtio_blk_ioreq *io = br->param;
	struct virtio_blk *blk = io->blk;
	struct virt_queue *vq;

	vq = &blk->virtio_dev.vqs[0];

	/* convert errno into a virtio block error return */
	if (err == EOPNOTSUPP || err == ENOSYS)
		*io->status = VIRTIO_BLK_S_UNSUPP;
	else if (err != 0)
		*io->status = VIRTIO_BLK_S_IOERR;
	else
		*io->status = VIRTIO_BLK_S_OK;

	/*
	 * Return the descriptor back to the host.
	 * We wrote 1 byte (our status) to host.
	 */
	pthread_mutex_lock(&blk->mtx);
	virtq_add_used_and_signal(vq, io->idx, 1);
	pthread_mutex_unlock(&blk->mtx);
}

static void
virtio_blk_proc(struct virtio_blk *blk,
		struct virt_queue *vq, uint16_t idx, int n)
{
	struct virtio_blk_hdr *vbh;
	struct virtio_blk_ioreq *io;
	int i;
	int err;
	ssize_t iolen;
	int __unused writeop, type;
	struct iovec *iov = vq->iovec;

	/*
	 * The first descriptor will be the read-only fixed header,
	 * and the last is for status (hence +2 above and below).
	 * The remaining iov's are the actual data I/O vectors.
	 *
	 * XXX - note - this fails on crash dump, which does a
	 * VIRTIO_BLK_T_FLUSH with a zero transfer length
	 */
	if (n < 2 || n > BLOCKIF_IOV_MAX + 2) {
		pr_err("wrong desc value\n");
		return;
	}

	io = &blk->ios[idx];
	if (iov[0].iov_len != sizeof(struct virtio_blk_hdr)) {
		pr_err("wrong size of virtio_blk_hdr %ld %ld\n",
				iov[0].iov_len, sizeof(struct virtio_blk_hdr));
		return;
	}

	vbh = iov[0].iov_base;
	memcpy(&io->req.iov, &iov[1], sizeof(struct iovec) * (n - 2));
	io->req.iovcnt = n - 2;
	io->req.offset = vbh->sector * DEV_BSIZE;
	io->status = iov[--n].iov_base;
	assert(iov[n].iov_len == 1);

	/*
	 * XXX
	 * The guest should not be setting the BARRIER flag because
	 * we don't advertise the capability.
	 */
	type = vbh->type & ~VBH_FLAG_BARRIER;
	writeop = (type == VBH_OP_WRITE);

	iolen = 0;
	for (i = 1; i < n; i++) {
		/*
		 * - write op implies read-only descriptor,
		 * - read/ident op implies write-only descriptor,
		 * therefore test the inverse of the descriptor bit
		 * to the op.
		 */
		iolen += iov[i].iov_len;
	}
	io->req.resid = iolen;

	DBG("virtio-block: %s op, %zd bytes, %d segs, offset %ld\n\r",
		 writeop ? "write" : "read/ident", iolen, i - 1,
		 io->req.offset);

	switch (type) {
	case VBH_OP_READ:
		err = blockif_read(blk->bc, &io->req);
		break;
	case VBH_OP_WRITE:
		err = blockif_write(blk->bc, &io->req);
		break;
	case VBH_OP_FLUSH:
	case VBH_OP_FLUSH_OUT:
		err = blockif_flush(blk->bc, &io->req);
		break;
	case VBH_OP_IDENT:
		/* Assume a single buffer */
		/* S/n equal to buffer is not zero-terminated. */
		memset(iov[1].iov_base, 0, iov[1].iov_len);
		strncpy(iov[1].iov_base, blk->ident,
		    MIN(iov[1].iov_len, sizeof(blk->ident)));
		virtio_blk_done(&io->req, 0);
		return;
	default:
		virtio_blk_done(&io->req, EOPNOTSUPP);
		return;
	}
	assert(err == 0);
}

static void
virtio_blk_notify(struct virt_queue *vq)
{
	int idx;
	unsigned int in, out;
	struct virtio_blk *blk;

	blk = virtio_dev_to_blk(vq->dev);
	virtq_disable_notify(vq);

	while (virtq_has_descs(vq)) {
		idx = virtq_get_descs(vq, vq->iovec,
				vq->iovec_size, &in, &out);
		if (idx < 0)
			return;

		if (idx == vq->num) {
			if (virtq_enable_notify(vq)) {
				virtq_disable_notify(vq);
				continue;
			}
			break;
		}

		if (in) {
			pr_err("unexpected description from guest");
			return;
		}

		virtio_blk_proc(blk, vq, idx, out);
	}
}

static int vblk_init_vq(struct virt_queue *vq)
{
	if (vq->vq_index == 0)
		vq->callback = virtio_blk_notify;
	else
		pr_err("virtio block only have one vq");

	return 0;
}

static struct virtio_ops vblk_ops = {
	.vq_init = vblk_init_vq,
};

static int
virtio_blk_init(struct vdev *vdev, char *opts)
{
	char bident[16];
	struct blockif_ctxt *bctxt;
	struct virtio_blk *blk;
	off_t size;
	int i, sectsz, sts, sto;
	pthread_mutexattr_t attr;
	int rc;

	if (opts == NULL || opts[0] == 0) {
		printf("virtio-block: backing device required\n");
		return -1;
	}

	/*
	 * The supplied backing file has to exist
	 */
	snprintf(bident, sizeof(bident), "%d:%d", 0, 0);
	bctxt = blockif_open(opts, bident);
	if (bctxt == NULL) {
		perror("Could not open backing file");
		return -1;
	}

	size = blockif_size(bctxt);
	sectsz = blockif_sectsz(bctxt);
	blockif_psectsz(bctxt, &sts, &sto);

	blk = calloc(1, sizeof(struct virtio_blk));
	if (!blk) {
		pr_warn("virtio_blk: calloc returns NULL\n");
		return -1;
	}

	/* virtio blk only has 1 virt queue */
	rc = virtio_device_init(&blk->virtio_dev, vdev,
			VIRTIO_TYPE_BLOCK, 1, VIRTIO_BLK_RINGSZ,
			VIRTIO_BLK_IOVSZ);
	if (rc) {
		pr_err("failed to init virtio blk device\n");
		free(blk);
		blockif_close(bctxt);
		return rc;
	}

	blk->bc = bctxt;
	for (i = 0; i < VIRTIO_BLK_RINGSZ; i++) {
		struct virtio_blk_ioreq *io = &blk->ios[i];

		io->req.callback = virtio_blk_done;
		io->req.param = io;
		io->blk = blk;
		io->idx = i;
	}

	vdev_set_pdata(vdev, blk);
	blk->virtio_dev.ops = &vblk_ops;
	blk->cfg = (struct virtio_blk_config *)blk->virtio_dev.config;


	/* init mutex attribute properly to avoid deadlock */
	rc = pthread_mutexattr_init(&attr);
	if (rc)
		pr_info("mutexattr init failed with erro %d!\n", rc);
	rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	if (rc)
		pr_info("virtio_blk: mutexattr_settype failed with "
					"error %d!\n", rc);

	rc = pthread_mutex_init(&blk->mtx, &attr);
	if (rc)
		pr_info("virtio_blk: pthread_mutex_init failed with "
					"error %d!\n", rc);

	sprintf(blk->ident, "Minos--%02X%02X-%02X%02X-%02X%02X",
			0, 1, 2, 3, 4, 5);

	/* setup virtio block config space */
	blk->cfg->capacity = size / DEV_BSIZE; /* 512-byte units */
	blk->cfg->size_max = 0;	/* not negotiated */
	blk->cfg->seg_max = BLOCKIF_IOV_MAX;
	blk->cfg->geometry.cylinders = 0;	/* no geometry */
	blk->cfg->geometry.heads = 0;
	blk->cfg->geometry.sectors = 0;
	blk->cfg->blk_size = sectsz;
	blk->cfg->topology.physical_block_exp =
	    (sts > sectsz) ? (ffsll(sts / sectsz) - 1) : 0;
	blk->cfg->topology.alignment_offset =
	    (sto != 0) ? ((sts - sto) / sectsz) : 0;
	blk->cfg->topology.min_io_size = 0;
	blk->cfg->topology.opt_io_size = 0;
	blk->cfg->writeback = blockif_get_wce(blk->bc);
	blk->original_wce = blk->cfg->writeback; /* save for reset */

	/* set the feature of the virtio block */
	if (!!blk->cfg->writeback) {
		virtio_set_feature(&blk->virtio_dev, VIRTIO_BLK_F_FLUSH);
		virtio_set_feature(&blk->virtio_dev, VIRTIO_BLK_F_CONFIG_WCE);
	}

	virtio_set_feature(&blk->virtio_dev, VIRTIO_F_VERSION_1);
	virtio_set_feature(&blk->virtio_dev, VIRTIO_BLK_F_SEG_MAX);
	virtio_set_feature(&blk->virtio_dev, VIRTIO_BLK_F_BLK_SIZE);
	virtio_set_feature(&blk->virtio_dev, VIRTIO_BLK_F_TOPOLOGY);
	virtio_set_feature(&blk->virtio_dev,
			VIRTIO_RING_F_INDIRECT_DESC);
	return 0;
}

static void
virtio_blk_deinit(struct vdev *vdev)
{
	struct blockif_ctxt *bctxt;
	struct virtio_blk *blk;

	blk = (struct virtio_blk *)vdev_get_pdata(vdev);
	if (!blk)
		return;

	pr_info("virtio_blk: deinit\n");
	bctxt = blk->bc;
	if (blockif_flush_all(bctxt)) {
		pr_warn("vrito_blk:"
			"Failed to flush before close\n");
		blockif_close(bctxt);
		virtio_device_deinit(&blk->virtio_dev);
		free(blk);
	}
}

static int virtio_blk_event(struct vdev *vdev, int read,
		unsigned long addr, unsigned long *value)
{
	struct virtio_blk *vblk;

	if (!vdev)
		return -EINVAL;

	vblk = (struct virtio_blk *)vdev_get_pdata(vdev);
	if (!vblk)
		return -EINVAL;

	return virtio_handle_mmio(&vblk->virtio_dev,
			read, addr, value);
}

static int virtio_blk_reset(struct vdev *vdev)
{
	struct virtio_blk *blk;

	blk = (struct virtio_blk *)vdev_get_pdata(vdev);
	if (!blk)
		return -EINVAL;

	pr_info("virtio_blk: device reset requested !\n");
	virtio_device_reset(&blk->virtio_dev);
	blockif_set_wce(blk->bc, blk->original_wce);

	return 0;
}

struct vdev_ops virtio_blk_ops = {
	.name		= "virtio_blk",
	.init		= virtio_blk_init,
	.deinit		= virtio_blk_deinit,
	.reset		= virtio_blk_reset,
	.handle_event	= virtio_blk_event,
};

DEFINE_VDEV_TYPE(virtio_blk_ops);
