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

#include <sys/uio.h>
#include <net/ethernet.h>
#ifndef NETMAP_WITH_LIBS
#define NETMAP_WITH_LIBS
#endif
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <linux/if_tun.h>

#include <mvm.h>
#include <mevent.h>
#include <virtio.h>
#include <netmap_user.h>
#include <barrier.h>

#define VIRTIO_NET_RINGSZ	1024
#define VIRTIO_NET_MAXSEGS	256

/*
 * Host capabilities.  Note that we only offer a few of these.
 */
#define	VIRTIO_NET_F_CSUM		(0) /* host handles partial cksum */
#define	VIRTIO_NET_F_GUEST_CSUM		(1) /* guest handles partial cksum */
#define	VIRTIO_NET_F_MAC		(5) /* host supplies MAC */
#define	VIRTIO_NET_F_GSO_DEPREC		(6) /* deprecated: host handles GSO */
#define	VIRTIO_NET_F_GUEST_TSO4		(7) /* guest can rcv TSOv4 */
#define	VIRTIO_NET_F_GUEST_TSO6		(8) /* guest can rcv TSOv6 */
#define	VIRTIO_NET_F_GUEST_ECN		(9) /* guest can rcv TSO with ECN */
#define	VIRTIO_NET_F_GUEST_UFO		(10) /* guest can rcv UFO */
#define	VIRTIO_NET_F_HOST_TSO4		(11) /* host can rcv TSOv4 */
#define	VIRTIO_NET_F_HOST_TSO6		(12) /* host can rcv TSOv6 */
#define	VIRTIO_NET_F_HOST_ECN		(13) /* host can rcv TSO with ECN */
#define	VIRTIO_NET_F_HOST_UFO		(14) /* host can rcv UFO */
#define	VIRTIO_NET_F_MRG_RXBUF		(15) /* host can merge RX buffers */
#define	VIRTIO_NET_F_STATUS		(16) /* config status field available */
#define	VIRTIO_NET_F_CTRL_VQ		(17) /* control channel available */
#define	VIRTIO_NET_F_CTRL_RX		(18) /* control channel RX mode support */
#define	VIRTIO_NET_F_CTRL_VLAN		(19) /* control channel VLAN filtering */
#define	VIRTIO_NET_F_GUEST_ANNOUN	(21) /* guest can send gratuitous pkts */

#define VIRTIO_NET_S_HOSTCAPS      \
	(VIRTIO_NET_F_MAC | VIRTIO_NET_F_MRG_RXBUF | VIRTIO_NET_F_STATUS | \
	VIRTIO_F_NOTIFY_ON_EMPTY | VIRTIO_RING_F_INDIRECT_DESC)

/* is address mcast/bcast? */
#define ETHER_IS_MULTICAST(addr) (*(addr) & 0x01)

/*
 * PCI config-space "registers"
 */
struct virtio_net_config {
	uint8_t  mac[6];
	uint16_t status;
} __attribute__((packed));

/*
 * Queue definitions.
 */
#define VIRTIO_NET_RXQ	0
#define VIRTIO_NET_TXQ	1
#define VIRTIO_NET_CTLQ	2	/* NB: not yet supported */

#define VIRTIO_NET_MAXQ	3

/*
 * Fixed network header size
 */
struct virtio_net_rxhdr {
	uint8_t		vrh_flags;
	uint8_t		vrh_gso_type;
	uint16_t	vrh_hdr_len;
	uint16_t	vrh_gso_size;
	uint16_t	vrh_csum_start;
	uint16_t	vrh_csum_offset;
	uint16_t	vrh_bufs;
} __attribute__((packed));

/*
 * Per-device struct
 */
struct virtio_net {
	struct virtio_device virtio_dev;
	pthread_mutex_t mtx;
	struct mevent	*mevp;

	int		tapfd;
	struct nm_desc	*nmd;

	int		rx_ready;

	volatile int	resetting;	/* set and checked outside lock */
	volatile int	closing;	/* stop the tx i/o thread */

	uint64_t	features;	/* negotiated features */

	struct virtio_net_config *config;

	pthread_mutex_t	rx_mtx;
	int		rx_in_progress;
	int		rx_vhdrlen;
	int		rx_merge;	/* merged rx bufs in use */
	pthread_t	tx_tid;
	pthread_mutex_t	tx_mtx;
	pthread_cond_t	tx_cond;
	int		tx_in_progress;

	void (*virtio_net_rx)(struct virtio_net *net);
	void (*virtio_net_tx)(struct virtio_net *net, struct iovec *iov,
			     int iovcnt, int len);
};

#define virtio_dev_to_net(dev) \
	(struct virtio_net *)container_of((dev), \
			struct virtio_net, virtio_dev);

static struct ether_addr *
ether_aton(const char *a, struct ether_addr *e)
{
	int i;
	unsigned int o0, o1, o2, o3, o4, o5;

	i = sscanf(a, "%x:%x:%x:%x:%x:%x", &o0, &o1, &o2, &o3, &o4, &o5);
	if (i != 6)
		return NULL;
	e->ether_addr_octet[0] = o0;
	e->ether_addr_octet[1] = o1;
	e->ether_addr_octet[2] = o2;
	e->ether_addr_octet[3] = o3;
	e->ether_addr_octet[4] = o4;
	e->ether_addr_octet[5] = o5;
	return e;
}

/*
 * If the transmit thread is active then stall until it is done.
 */
static void
virtio_net_txwait(struct virtio_net *net)
{
	pthread_mutex_lock(&net->tx_mtx);
	while (net->tx_in_progress) {
		pthread_mutex_unlock(&net->tx_mtx);
		usleep(10000);
		pthread_mutex_lock(&net->tx_mtx);
	}
	pthread_mutex_unlock(&net->tx_mtx);
}

/*
 * If the receive thread is active then stall until it is done.
 */
static void
virtio_net_rxwait(struct virtio_net *net)
{
	pthread_mutex_lock(&net->rx_mtx);
	while (net->rx_in_progress) {
		pthread_mutex_unlock(&net->rx_mtx);
		usleep(10000);
		pthread_mutex_lock(&net->rx_mtx);
	}
	pthread_mutex_unlock(&net->rx_mtx);
}

/*
 * Send signal to tx I/O thread and wait till it exits
 */
static void
virtio_net_tx_stop(struct virtio_net *net)
{
	void *jval;

	net->closing = 1;

	pthread_cond_broadcast(&net->tx_cond);

	pthread_join(net->tx_tid, &jval);
}

/*
 * Called to send a buffer chain out to the tap device
 */
static void virtio_net_tap_tx(struct virtio_net *net,
		struct iovec *iov, int iovcnt, int len)
{
	static char pad[60]; /* all zero bytes */
	ssize_t ret;

	if (net->tapfd == -1)
		return;

	/*
	 * If the length is < 60, pad out to that and add the
	 * extra zero'd segment to the iov. It is guaranteed that
	 * there is always an extra iov available by the caller.
	 */
	if (len < 60) {
		iov[iovcnt].iov_base = pad;
		iov[iovcnt].iov_len = 60 - len;
		iovcnt++;
	}
	ret = writev(net->tapfd, iov, iovcnt);
	(void)ret; /*avoid compiler warning*/
}

/*
 *  Called when there is read activity on the tap file descriptor.
 * Each buffer posted by the guest is assumed to be able to contain
 * an entire ethernet frame + rx header.
 *  MP note: the dummybuf is only used for discarding frames, so there
 * is no need for it to be per-vtnet or locked.
 */
static uint8_t dummybuf[2048];

static inline struct iovec *
rx_iov_trim(struct iovec *iov, int *niov, int tlen)
{
	struct iovec *riov;

	/* XXX short-cut: assume first segment is >= tlen */
	assert(iov[0].iov_len >= tlen);

	iov[0].iov_len -= tlen;
	if (iov[0].iov_len == 0) {
		assert(*niov > 1);
		*niov -= 1;
		riov = &iov[1];
	} else {
		iov[0].iov_base = (void *)((uintptr_t)iov[0].iov_base + tlen);
		riov = &iov[0];
	}

	return riov;
}

static void
virtio_net_tap_rx(struct virtio_net *net)
{
	struct iovec iov[VIRTIO_NET_MAXSEGS], *riov;
	struct virt_queue *vq;
	void *vrx;
	int len, n;
	uint16_t idx;
	ssize_t ret;
	unsigned int in, out;

	/*
	 * Should never be called without a valid tap fd
	 */
	assert(net->tapfd != -1);

	/*
	 * But, will be called when the rx ring hasn't yet
	 * been set up or the guest is resetting the device.
	 */
	if (!net->rx_ready || net->resetting) {
		/*
		 * Drop the packet and try later.
		 */
		ret = read(net->tapfd, dummybuf, sizeof(dummybuf));
		(void)ret; /*avoid compiler warning*/

		return;
	}

	/*
	 * Check for available rx buffers
	 */
	vq = &net->virtio_dev.vqs[VIRTIO_NET_RXQ];
	if (!virtq_has_descs(vq)) {
		/*
		 * Drop the packet and try later.  Interrupt on
		 * empty, if that's negotiated.
		 */
		ret = read(net->tapfd, dummybuf, sizeof(dummybuf));
		(void)ret; /*avoid compiler warning*/

		virtq_notify(vq);
		return;
	}

	do {
		/*
		 * Get descriptor chain.
		 */
		idx = virtq_get_descs(vq, vq->iovec,
				VIRTIO_NET_MAXSEGS, &in, &out);

		assert(in >= 1 && in <= VIRTIO_NET_MAXSEGS);

		/*
		 * Get a pointer to the rx header, and use the
		 * data immediately following it for the packet buffer.
		 */
		vrx = iov[0].iov_base;
		riov = rx_iov_trim(iov, &n, net->rx_vhdrlen);

		len = readv(net->tapfd, riov, n);

		if (len < 0 && errno == EWOULDBLOCK) {
			/*
			 * No more packets, but still some avail ring
			 * entries.  Interrupt if needed/appropriate.
			 */
			virtq_discard_desc(vq, 1);
			virtq_notify(vq);
			return;
		}

		/*
		 * The only valid field in the rx packet header is the
		 * number of buffers if merged rx bufs were negotiated.
		 */
		memset(vrx, 0, net->rx_vhdrlen);

		if (net->rx_merge) {
			struct virtio_net_rxhdr *vrxh;

			vrxh = vrx;
			vrxh->vrh_bufs = 1;
		}

		/*
		 * Release this chain and handle more chains.
		 */
		virtq_add_used_and_signal(vq, idx, len + net->rx_vhdrlen);
	} while (virtq_has_descs(vq));

	/* Interrupt if needed, including for NOTIFY_ON_EMPTY. */
	virtq_notify(vq);
}

static inline int
virtio_net_netmap_writev(struct nm_desc *nmd, struct iovec *iov, int iovcnt)
{
	int r, i;
	int len = 0;

	for (r = nmd->cur_tx_ring; ; ) {
		struct netmap_ring *ring = NETMAP_TXRING(nmd->nifp, r);
		uint32_t cur, idx;
		char *buf;

		if (nm_ring_empty(ring)) {
			r++;
			if (r > nmd->last_tx_ring)
				r = nmd->first_tx_ring;
			if (r == nmd->cur_tx_ring)
				break;
			continue;
		}
		cur = ring->cur;
		idx = ring->slot[cur].buf_idx;
		buf = NETMAP_BUF(ring, idx);

		for (i = 0; i < iovcnt; i++) {
			if (len + iov[i].iov_len > 2048)
				break;
			memcpy(&buf[len], iov[i].iov_base, iov[i].iov_len);
			len += iov[i].iov_len;
		}
		ring->slot[cur].len = len;
		ring->head = ring->cur = nm_ring_next(ring, cur);
		nmd->cur_tx_ring = r;
		ioctl(nmd->fd, NIOCTXSYNC, NULL);
		break;
	}

	return len;
}

static inline int
virtio_net_netmap_readv(struct nm_desc *nmd, struct iovec *iov, int iovcnt)
{
	int len = 0;
	int i = 0;
	int r;

	for (r = nmd->cur_rx_ring; ; ) {
		struct netmap_ring *ring = NETMAP_RXRING(nmd->nifp, r);
		uint32_t cur, idx;
		char *buf;
		size_t left;

		if (nm_ring_empty(ring)) {
			r++;
			if (r > nmd->last_rx_ring)
				r = nmd->first_rx_ring;
			if (r == nmd->cur_rx_ring)
				break;
			continue;
		}
		cur = ring->cur;
		idx = ring->slot[cur].buf_idx;
		buf = NETMAP_BUF(ring, idx);
		left = ring->slot[cur].len;

		for (i = 0; i < iovcnt && left > 0; i++) {
			if (iov[i].iov_len > left)
				iov[i].iov_len = left;
			memcpy(iov[i].iov_base, &buf[len], iov[i].iov_len);
			len += iov[i].iov_len;
			left -= iov[i].iov_len;
		}
		ring->head = ring->cur = nm_ring_next(ring, cur);
		nmd->cur_rx_ring = r;
		ioctl(nmd->fd, NIOCRXSYNC, NULL);
		break;
	}
	for (; i < iovcnt; i++)
		iov[i].iov_len = 0;

	return len;
}

/*
 * Called to send a buffer chain out to the vale port
 */
static void
virtio_net_netmap_tx(struct virtio_net *net, struct iovec *iov, int iovcnt,
		    int len)
{
	static char pad[60]; /* all zero bytes */

	if (net->nmd == NULL)
		return;

	/*
	 * If the length is < 60, pad out to that and add the
	 * extra zero'd segment to the iov. It is guaranteed that
	 * there is always an extra iov available by the caller.
	 */
	if (len < 60) {
		iov[iovcnt].iov_base = pad;
		iov[iovcnt].iov_len = 60 - len;
		iovcnt++;
	}
	(void) virtio_net_netmap_writev(net->nmd, iov, iovcnt);
}

static void
virtio_net_netmap_rx(struct virtio_net *net)
{
	struct iovec iov[VIRTIO_NET_MAXSEGS], *riov;
	struct virt_queue *vq;
	void *vrx;
	int len, n;
	uint16_t idx;
	unsigned int in, out;

	/*
	 * Should never be called without a valid netmap descriptor
	 */
	assert(net->nmd != NULL);

	/*
	 * But, will be called when the rx ring hasn't yet
	 * been set up or the guest is resetting the device.
	 */
	if (!net->rx_ready || net->resetting) {
		/*
		 * Drop the packet and try later.
		 */
		(void) nm_nextpkt(net->nmd, (void *)dummybuf);
		return;
	}

	/*
	 * Check for available rx buffers
	 */
	vq = &net->virtio_dev.vqs[VIRTIO_NET_RXQ];
	if (!virtq_has_descs(vq)) {
		/*
		 * Drop the packet and try later.  Interrupt on
		 * empty, if that's negotiated.
		 */
		(void) nm_nextpkt(net->nmd, (void *)dummybuf);
		virtq_notify(vq);
		return;
	}

	virtq_disable_notify(vq);

	for (;;) {
		/*
		 * Get descriptor chain.
		 */
		idx = virtq_get_descs(vq, vq->iovec,
				VIRTIO_NET_MAXSEGS, &in, &out);
		if (idx < 0)
			return;

		if (idx == vq->num) {
			if (virtq_enable_notify(vq)) {
				virtq_disable_notify(vq);
				continue;
			}
			break;
		}

		assert(in >= 1 && in <= VIRTIO_NET_MAXSEGS);

		/*
		 * Get a pointer to the rx header, and use the
		 * data immediately following it for the packet buffer.
		 */
		vrx = iov[0].iov_base;
		riov = rx_iov_trim(iov, &n, net->rx_vhdrlen);

		len = virtio_net_netmap_readv(net->nmd, riov, n);

		if (len == 0) {
			/*
			 * No more packets, but still some avail ring
			 * entries.  Interrupt if needed/appropriate.
			 */
			virtq_notify(vq);
			return;
		}

		/*
		 * The only valid field in the rx packet header is the
		 * number of buffers if merged rx bufs were negotiated.
		 */
		memset(vrx, 0, net->rx_vhdrlen);

		if (net->rx_merge) {
			struct virtio_net_rxhdr *vrxh;

			vrxh = vrx;
			vrxh->vrh_bufs = 1;
		}

		/*
		 * Release this chain and handle more chains.
		 */
		virtq_add_used_and_signal(vq, idx, len + net->rx_vhdrlen);
	}

	/* Interrupt if needed, including for NOTIFY_ON_EMPTY. */
	virtq_notify(vq);
}

static void
virtio_net_rx_callback(int fd, enum ev_type type, void *param)
{
	struct virtio_net *net = param;

	pthread_mutex_lock(&net->rx_mtx);
	net->rx_in_progress = 1;
	net->virtio_net_rx(net);
	net->rx_in_progress = 0;
	pthread_mutex_unlock(&net->rx_mtx);
}

static void
virtio_net_ping_rxq(struct virt_queue *vq)
{
	struct virtio_net *net = virtio_dev_to_net(vq->dev);

	/*
	 * A qnotify means that the rx process can now begin
	 */
	if (net->rx_ready == 0) {
		net->rx_ready = 1;
		vq->used->flags |= VRING_USED_F_NO_NOTIFY;
	}
}

static void
virtio_net_proctx(struct virtio_net *net, struct virt_queue *vq)
{
	int i;
	int plen, tlen;
	uint16_t idx;
	unsigned int in, out;

	/*
	 * Obtain chain of descriptors.  The first one is
	 * really the header descriptor, so we need to sum
	 * up two lengths: packet length and transfer length.
	 */
	idx = virtq_get_descs(vq, vq->iovec, VIRTIO_NET_MAXSEGS, &in, &out);
	if (idx < 0)
		return;

	if (idx == vq->num)
		return;

	assert( out >= 1 && out <= VIRTIO_NET_MAXSEGS);
	plen = 0;
	tlen = vq->iovec[0].iov_len;
	for (i = 1; i < out; i++) {
		plen += vq->iovec[i].iov_len;
		tlen += vq->iovec[i].iov_len;
	}

	pr_info("virtio: packet send, %d bytes, %d segs\n\r", plen, out);
	net->virtio_net_tx(net, &vq->iovec[1], out - 1, plen);

	/* chain is processed, release it and set tlen */
	virtq_add_used(vq, idx, tlen);
}

static void
virtio_net_ping_txq(struct virt_queue *vq)
{
	struct virtio_net *net = virtio_dev_to_net(vq->dev);

	/*
	 * Any ring entries to process?
	 */
	if (!virtq_has_descs(vq))
		return;

	/* Signal the tx thread for processing */
	pthread_mutex_lock(&net->tx_mtx);
	vq->used->flags |= VRING_USED_F_NO_NOTIFY;
	if (net->tx_in_progress == 0)
		pthread_cond_signal(&net->tx_cond);
	pthread_mutex_unlock(&net->tx_mtx);
}

/*
 * Thread which will handle processing of TX desc
 */
static void *
virtio_net_tx_thread(void *param)
{
	struct virtio_net *net = param;
	struct virt_queue *vq;
	int error;

	vq = &net->virtio_dev.vqs[VIRTIO_NET_TXQ];

	/*
	 * Let us wait till the tx queue pointers get initialised &
	 * first tx signaled
	 */
	pthread_mutex_lock(&net->tx_mtx);
	error = pthread_cond_wait(&net->tx_cond, &net->tx_mtx);
	assert(error == 0);
	if (net->closing) {
		pr_warn("vtnet tx thread closing...\n");
		pthread_mutex_unlock(&net->tx_mtx);
		return NULL;
	}

	for (;;) {
		/* note - tx mutex is locked here */
		while (net->resetting || !virtq_has_descs(vq)) {
			virtq_enable_notify(vq);
			/* memory barrier */
			dsb(sy);
			if (!net->resetting && virtq_has_descs(vq))
				break;

			net->tx_in_progress = 0;
			error = pthread_cond_wait(&net->tx_cond, &net->tx_mtx);
			assert(error == 0);
			if (net->closing) {
				pr_warn("vtnet tx thread closing...\n");
				pthread_mutex_unlock(&net->tx_mtx);
				return NULL;
			}
		}

		virtq_disable_notify(vq);
		net->tx_in_progress = 1;
		pthread_mutex_unlock(&net->tx_mtx);

		do {
			/*
			 * Run through entries, placing them into
			 * iovecs and sending when an end-of-packet
			 * is found
			 */
			virtio_net_proctx(net, vq);
		} while (virtq_has_descs(vq));

		/*
		 * Generate an interrupt if needed.
		 */
		virtq_enable_notify(vq);
		virtq_notify(vq);

		pthread_mutex_lock(&net->tx_mtx);
	}
}

#ifdef notyet
static int
virtio_net_ping_ctlq(struct virt_queue *vq)
{
	pr_info("vtnet: control qnotify!\n\r");
	return 0;
}
#endif

static int
virtio_net_parsemac(char *mac_str, uint8_t *mac_addr)
{
	struct ether_addr ether_addr;
	struct ether_addr *ea;
	char *tmpstr;
	char zero_addr[ETHER_ADDR_LEN] = { 0, 0, 0, 0, 0, 0 };

	tmpstr = strsep(&mac_str, "=");
	ea = &ether_addr;
	if ((mac_str != NULL) && (!strcmp(tmpstr, "mac"))) {
		ea = ether_aton(mac_str, ea);

		if (ea == NULL || ETHER_IS_MULTICAST(ea->ether_addr_octet) ||
		    memcmp(ea->ether_addr_octet, zero_addr, ETHER_ADDR_LEN)
				== 0) {
			fprintf(stderr, "Invalid MAC %s\n", mac_str);
			return -1;
		}
		memcpy(mac_addr, ea->ether_addr_octet, ETHER_ADDR_LEN);
	}

	return 0;
}

static int
virtio_net_tap_open(char *devname)
{
	int tunfd, rc;
	struct ifreq ifr;

#define PATH_NET_TUN "/dev/net/tun"
	tunfd = open(PATH_NET_TUN, O_RDWR);
	if (tunfd < 0) {
		pr_warn("open of tup device /dev/net/tun failed\n");
		return -1;
	}

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = IFF_TAP | IFF_NO_PI;

	if (*devname)
		strncpy(ifr.ifr_name, devname, IFNAMSIZ);

	rc = ioctl(tunfd, TUNSETIFF, (void *)&ifr);
	if (rc < 0) {
		pr_warn("open of tap device %s failed\n", devname);
		close(tunfd);
		return -1;
	}

	strcpy(devname, ifr.ifr_name);
	return tunfd;
}

static void
virtio_net_tap_setup(struct virtio_net *net, char *devname)
{
	char tbuf[80 + 6];	/* room for "minos_" prefix */
	char *tbuf_ptr;

	tbuf_ptr = tbuf;

	strcpy(tbuf, "minos_");

	tbuf_ptr += 6;

	strncat(tbuf_ptr, devname, sizeof(tbuf) - 7);

	net->virtio_net_rx = virtio_net_tap_rx;
	net->virtio_net_tx = virtio_net_tap_tx;

	net->tapfd = virtio_net_tap_open(tbuf);
	if (net->tapfd == -1) {
		pr_warn("open of tap device %s failed\n", tbuf);
		return;
	}
	pr_info("open of tap device %s success!\n", tbuf);

	/*
	 * Set non-blocking and register for read
	 * notifications with the event loop
	 */
	int opt = 1;

	if (ioctl(net->tapfd, FIONBIO, &opt) < 0) {
		pr_warn("tap device O_NONBLOCK failed\n");
		close(net->tapfd);
		net->tapfd = -1;
	}

	net->mevp = mevent_add(net->tapfd, EVF_READ,
			       virtio_net_rx_callback, net);
	if (net->mevp == NULL) {
		pr_warn("Could not register event\n");
		close(net->tapfd);
		net->tapfd = -1;
	}
}

static void
virtio_net_netmap_setup(struct virtio_net *net, char *ifname)
{
	net->virtio_net_rx = virtio_net_netmap_rx;
	net->virtio_net_tx = virtio_net_netmap_tx;

	net->nmd = nm_open(ifname, NULL, 0, 0);
	if (net->nmd == NULL) {
		pr_warn("open of netmap device %s failed\n", ifname);
		return;
	}

	net->mevp = mevent_add(net->nmd->fd, EVF_READ,
			       virtio_net_rx_callback, net);
	if (net->mevp == NULL) {
		pr_warn("Could not register event\n");
		nm_close(net->nmd);
		net->nmd = NULL;
	}
}

static void virtio_net_neg_features(struct virtio_device *dev)
{
	struct virtio_net *net;

	net = virtio_dev_to_net(dev);
	net->features = dev->acked_features;

	if (!(net->features & VIRTIO_NET_F_MRG_RXBUF)) {
		net->rx_merge = 0;
		/* non-merge rx header is 2 bytes shorter */
		net->rx_vhdrlen -= 2;
	}
}

static int vnet_init_vq(struct virt_queue *vq)
{
	if (vq->vq_index == VIRTIO_NET_RXQ)
		vq->callback = virtio_net_ping_rxq;
	else if (vq->vq_index == VIRTIO_NET_TXQ)
		vq->callback = virtio_net_ping_txq;
#ifdef notyet
	else if (vq->vq_index == VIRTIO_NET_CTLQ)
		vq->callback = virtio_net_ping_ctlq;
#endif
	else
		pr_err("unsupported vq index %d\n", vq->vq_index);

	return 0;
}

static struct virtio_ops vnet_ops = {
	.vq_init = vnet_init_vq,
	.neg_features = virtio_net_neg_features,
};

static int virtio_net_init(struct vdev *vdev, char *opts)
{
	char nstr[80];
	char tname[MAXCOMLEN + 1];
	struct virtio_net *net;
	char *devname;
	char *vtopts;
	int mac_provided;
	pthread_mutexattr_t attr;
	int rc;

	net = calloc(1, sizeof(struct virtio_net));
	if (!net) {
		pr_warn("virtio_net: calloc returns NULL\n");
		return -1;
	}

	rc = virtio_device_init(&net->virtio_dev, vdev,
			VIRTIO_TYPE_NET, VIRTIO_NET_MAXQ,
			VIRTIO_NET_RINGSZ);
	if (rc) {
		pr_err("failed to init virtio net device\n");
		free(net);
	}

	vdev_set_pdata(vdev, net);
	net->virtio_dev.ops = &vnet_ops;
	net->config = (struct virtio_net_config *)net->virtio_dev.config;

	/* init mutex attribute properly to avoid deadlock */
	rc = pthread_mutexattr_init(&attr);
	if (rc)
		pr_info("mutexattr init failed with erro %d!\n", rc);

	rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	if (rc)
		pr_info("virtio_net: mutexattr_settype failed with "
			"error %d!\n", rc);

	rc = pthread_mutex_init(&net->mtx, &attr);
	if (rc)
		pr_info("virtio_net: pthread_mutex_init failed with "
			"error %d!\n", rc);

	virtio_set_feature(&net->virtio_dev, VIRTIO_NET_F_MAC);
	virtio_set_feature(&net->virtio_dev, VIRTIO_NET_F_MRG_RXBUF);
	virtio_set_feature(&net->virtio_dev, VIRTIO_NET_F_STATUS);
	virtio_set_feature(&net->virtio_dev, VIRTIO_F_NOTIFY_ON_EMPTY);
	virtio_set_feature(&net->virtio_dev, VIRTIO_RING_F_INDIRECT_DESC);

	/*
	 * Attempt to open the tap device and read the MAC address
	 * if specified
	 */
	mac_provided = 0;
	net->tapfd = -1;
	net->nmd = NULL;
	if (opts != NULL) {
		int err;

		devname = vtopts = strdup(opts);
		if (!devname) {
			pr_warn("virtio_net: strdup returns NULL\n");
			goto error;
		}

		(void) strsep(&vtopts, ",");

		if (vtopts != NULL) {
			err = virtio_net_parsemac(vtopts, net->config->mac);
			if (err != 0) {
				free(devname);
				goto error;
			}
			mac_provided = 1;
		}

		if (strncmp(devname, "vale", 4) == 0)
			virtio_net_netmap_setup(net, devname);
		if (strncmp(devname, "tap", 3) == 0 ||
		    strncmp(devname, "vmnet", 5) == 0)
			virtio_net_tap_setup(net, devname);

		free(devname);
	}

	/*
	 * The default MAC address is the standard NetApp OUI of 00-a0-98,
	 * followed by an MD5 of the PCI slot/func number and dev name
	 */
	if (!mac_provided) {
		snprintf(nstr, sizeof(nstr), "%d-%d-%s",
				0x0, 0x1, "minos");
		net->config->mac[0] = 0x00;
		net->config->mac[1] = 0x16;
		net->config->mac[2] = 0x3E;
		net->config->mac[3] = 0x11;
		net->config->mac[4] = 0x22;
		net->config->mac[5] = 0x33;
	}

	/* Link is up if we managed to open tap device or vale port. */
	net->config->status = (opts == NULL || net->tapfd >= 0 ||
			      net->nmd != NULL);

	net->resetting = 0;
	net->closing = 0;

	net->rx_merge = 1;
	net->rx_vhdrlen = sizeof(struct virtio_net_rxhdr);
	net->rx_in_progress = 0;
	pthread_mutex_init(&net->rx_mtx, NULL);

	/*
	 * Initialize tx semaphore & spawn TX processing thread.
	 * As of now, only one thread for TX desc processing is
	 * spawned.
	 */
	net->tx_in_progress = 0;
	pthread_mutex_init(&net->tx_mtx, NULL);
	pthread_cond_init(&net->tx_cond, NULL);
	pthread_create(&net->tx_tid, NULL, virtio_net_tx_thread,
		       (void *)net);
	snprintf(tname, sizeof(tname), "vtnet-%d:%d tx", 0x0, 0x1);
	pthread_setname_np(net->tx_tid, tname);

	return 0;

error:
	virtio_device_deinit(&net->virtio_dev);
	free(net);

	return -EFAULT;
}

static void
virtio_net_deinit(struct vdev *vdev)
{
	struct virtio_net *net;

	net = (struct virtio_net *)vdev_get_pdata(vdev);
	if (!net) {
		fprintf(stderr, "%s: NULL!\n", __func__);
		return;
	}

	virtio_net_tx_stop(net);

	if (net->tapfd >= 0) {
		close(net->tapfd);
		net->tapfd = -1;
	} else
		fprintf(stderr, "net->tapfd is -1!\n");

	if (net->mevp != NULL)
		mevent_delete(net->mevp);

	virtio_device_deinit(&net->virtio_dev);
	free(net);

	pr_info("%s: done\n", __func__);
}

static int virtio_net_event(struct vdev *vdev, int read,
		unsigned long addr, unsigned long *value)
{
	struct virtio_net *net;

	net = (struct virtio_net *)vdev_get_pdata(vdev);
	if (!net)
		return -EINVAL;

	return virtio_handle_mmio(&net->virtio_dev,
			read, addr, value);
}

static int virtio_net_reset(struct vdev *vdev)
{
	struct virtio_net *net;

	net = (struct virtio_net *)vdev_get_pdata(vdev);
	if (!net)
		return -EINVAL;

	pr_info("vtnet: device reset requested !\n");

	net->resetting = 1;

	/*
	 * Wait for the transmit and receive threads to finish their
	 * processing.
	 */
	virtio_net_txwait(net);
	virtio_net_rxwait(net);

	net->rx_ready = 0;
	net->rx_merge = 1;
	net->rx_vhdrlen = sizeof(struct virtio_net_rxhdr);

	virtio_device_reset(&net->virtio_dev);

	net->resetting = 0;
	net->closing = 0;

	return 0;
}

struct vdev_ops virtio_net_ops = {
	.name		= "virtio-net",
	.init		= virtio_net_init,
	.deinit		= virtio_net_deinit,
	.reset		= virtio_net_reset,
	.handle_event	= virtio_net_event,
};
DEFINE_VDEV_TYPE(virtio_net_ops);
