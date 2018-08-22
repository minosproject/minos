/*-
 * Copyright (c) 2016 iXsystems Inc.
 * All rights reserved.
 *
 * This software was developed by Jakub Klama <jceel@FreeBSD.org>
 * under sponsorship from iXsystems Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <fcntl.h>
#include <string.h>
#include <mvm.h>
#include <virtio.h>
#include <mevent.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <errno.h>
#include <assert.h>

#include <compiler.h>

#define	VIRTIO_CONSOLE_RINGSZ	64
#define	VIRTIO_CONSOLE_MAXPORTS	16
#define	VIRTIO_CONSOLE_MAXQ	(VIRTIO_CONSOLE_MAXPORTS * 2 + 2)

#define	VIRTIO_CONSOLE_DEVICE_READY	0
#define	VIRTIO_CONSOLE_DEVICE_ADD	1
#define	VIRTIO_CONSOLE_DEVICE_REMOVE	2
#define	VIRTIO_CONSOLE_PORT_READY	3
#define	VIRTIO_CONSOLE_CONSOLE_PORT	4
#define	VIRTIO_CONSOLE_CONSOLE_RESIZE	5
#define	VIRTIO_CONSOLE_PORT_OPEN	6
#define	VIRTIO_CONSOLE_PORT_NAME	7

#define	VIRTIO_CONSOLE_F_SIZE		0
#define	VIRTIO_CONSOLE_F_MULTIPORT	1
#define	VIRTIO_CONSOLE_F_EMERG_WRITE	2
#define	VIRTIO_CONSOLE_S_HOSTCAPS	\
	(VIRTIO_CONSOLE_F_SIZE |	\
	VIRTIO_CONSOLE_F_MULTIPORT |	\
	VIRTIO_CONSOLE_F_EMERG_WRITE)

struct virtio_console;
struct virtio_console_port;
struct virtio_console_config;
typedef void (virtio_console_cb_t)(struct virtio_console_port *, void *,
				   struct iovec *, int);

enum virtio_console_be_type {
	VIRTIO_CONSOLE_BE_STDIO = 0,
	VIRTIO_CONSOLE_BE_TTY,
	VIRTIO_CONSOLE_BE_PTY,
	VIRTIO_CONSOLE_BE_FILE,
	VIRTIO_CONSOLE_BE_MAX,
	VIRTIO_CONSOLE_BE_INVALID = VIRTIO_CONSOLE_BE_MAX
};

struct virtio_console_port {
	struct virtio_console	*console;
	int			id;
	const char		*name;
	bool			enabled;
	bool			is_console;
	bool			rx_ready;
	bool			open;
	int			rxq;
	int			txq;
	void			*arg;
	virtio_console_cb_t	*cb;
};

struct virtio_console_backend {
	struct virtio_console_port	*port;
	struct mevent			*evp;
	int				fd;
	bool				open;
	enum virtio_console_be_type	be_type;
	int				pts_fd;	/* only valid for PTY */
};

struct virtio_console {
	struct virtio_device		virtio_dev;
	pthread_mutex_t			mtx;
	uint64_t			cfg;
	uint64_t			features;
	int				nports;
	bool				ready;
	struct virtio_console_port	control_port;
	struct virtio_console_port	ports[VIRTIO_CONSOLE_MAXPORTS];
	struct virtio_console_config	*config;
};

#define virtio_dev_to_console(dev) \
	(struct virtio_console *)container_of(dev, \
			struct virtio_console, virtio_dev)

struct virtio_console_config {
	uint16_t cols;
	uint16_t rows;
	uint32_t max_nr_ports;
	uint32_t emerg_wr;
} __attribute__((packed));

struct virtio_console_control {
	uint32_t id;
	uint16_t event;
	uint16_t value;
} __attribute__((packed));

struct virtio_console_console_resize {
	uint16_t cols;
	uint16_t rows;
} __attribute__((packed));

static int stdio_in_use;

extern int posix_openpt(int flags);
extern int grantpt(int fd);
extern int unlockpt(int fd);
static int virtio_console_notify_rx(struct virt_queue *);
static int virtio_console_notify_tx(struct virt_queue *);
static void virtio_console_control_send(struct virtio_console *,
	struct virtio_console_control *, const void *, size_t);
static void virtio_console_announce_port(struct virtio_console_port *);
static void virtio_console_open_port(struct virtio_console_port *, bool);

static const char *virtio_console_be_table[VIRTIO_CONSOLE_BE_MAX] = {
	[VIRTIO_CONSOLE_BE_STDIO]	= "stdio",
	[VIRTIO_CONSOLE_BE_TTY]		= "tty",
	[VIRTIO_CONSOLE_BE_PTY]		= "pty",
	[VIRTIO_CONSOLE_BE_FILE]	= "file"
};

static struct termios virtio_console_saved_tio;
static int virtio_console_saved_flags;

static int
virtio_console_reset(struct virtio_device *dev)
{
	pr_debug("vtcon: device reset requested!\n");
	virtio_device_reset(dev);

	return 0;
}

static int vcon_init_vq(struct virt_queue *vq)
{
	if ((vq->vq_index % 2) == 0)
		vq->callback = virtio_console_notify_rx;
	else
		vq->callback = virtio_console_notify_tx;

	return 0;
}

static struct virtio_ops vcon_ops = {
	.vq_init = vcon_init_vq,
	.reset	= virtio_console_reset,
};

static inline struct virtio_console_port *
virtio_console_vq_to_port(struct virtio_console *console,
			  struct virt_queue *vq)
{
	uint16_t num = vq->vq_index;

	if (num == 0 || num == 1)
		return &console->ports[0];

	if (num == 2 || num == 3)
		return &console->control_port;

	return &console->ports[(num / 2) - 1];
}

static inline struct virt_queue *
virtio_console_port_to_vq(struct virtio_console_port *port, bool tx_queue)
{
	int qnum;

	qnum = tx_queue ? port->txq : port->rxq;
	return &port->console->virtio_dev.vqs[qnum];
}

static struct virtio_console_port *
virtio_console_add_port(struct virtio_console *console, const char *name,
			virtio_console_cb_t *cb, void *arg, bool is_console)
{
	struct virtio_console_port *port;

	if (console->nports == VIRTIO_CONSOLE_MAXPORTS) {
		errno = EBUSY;
		return NULL;
	}

	port = &console->ports[console->nports++];
	port->id = console->nports - 1;
	port->console = console;
	port->name = name;
	port->cb = cb;
	port->arg = arg;
	port->is_console = is_console;

	if (port->id == 0) {
		/* port0 */
		port->txq = 0;
		port->rxq = 1;
	} else {
		port->txq = console->nports * 2;
		port->rxq = port->txq + 1;
	}

	port->enabled = true;
	return port;
}

static void
virtio_console_control_tx(struct virtio_console_port *port, void *arg,
			  struct iovec *iov, int niov)
{
	struct virtio_console *console;
	struct virtio_console_port *tmp;
	struct virtio_console_control resp, *ctrl;
	int i;

	console = port->console;
	ctrl = (struct virtio_console_control *)iov->iov_base;

	switch (ctrl->event) {
	case VIRTIO_CONSOLE_DEVICE_READY:
		console->ready = true;
		/* set port ready events for registered ports */
		for (i = 0; i < VIRTIO_CONSOLE_MAXPORTS; i++) {
			tmp = &console->ports[i];
			if (tmp->enabled)
				virtio_console_announce_port(tmp);

			if (tmp->open)
				virtio_console_open_port(tmp, true);
		}
		break;

	case VIRTIO_CONSOLE_PORT_READY:
		if (ctrl->id >= console->nports) {
			pr_warn("VTCONSOLE_PORT_READY for unknown port %d\n",
			    ctrl->id);
			return;
		}

		tmp = &console->ports[ctrl->id];
		if (tmp->is_console) {
			resp.event = VIRTIO_CONSOLE_CONSOLE_PORT;
			resp.id = ctrl->id;
			resp.value = 1;
			virtio_console_control_send(console, &resp, NULL, 0);
		}
		break;
	}
}

static void
virtio_console_announce_port(struct virtio_console_port *port)
{
	struct virtio_console_control event;

	event.id = port->id;
	event.event = VIRTIO_CONSOLE_DEVICE_ADD;
	event.value = 1;
	virtio_console_control_send(port->console, &event, NULL, 0);

	event.event = VIRTIO_CONSOLE_PORT_NAME;
	virtio_console_control_send(port->console, &event, port->name,
	    strlen(port->name));
}

static void
virtio_console_open_port(struct virtio_console_port *port, bool open)
{
	struct virtio_console_control event;

	if (!port->console->ready) {
		port->open = true;
		return;
	}

	event.id = port->id;
	event.event = VIRTIO_CONSOLE_PORT_OPEN;
	event.value = (int)open;
	virtio_console_control_send(port->console, &event, NULL, 0);
}

static void
virtio_console_control_send(struct virtio_console *console,
			    struct virtio_console_control *ctrl,
			    const void *payload, size_t len)
{
	struct virt_queue *vq;
	struct iovec iov;
	uint16_t idx;
	unsigned int in, out;

	vq = virtio_console_port_to_vq(&console->control_port, true);

	if (!virtq_has_descs(vq))
		return;

	idx = virtq_get_descs(vq, &iov, 1, &in, &out);

	memcpy(iov.iov_base, ctrl, sizeof(struct virtio_console_control));
	if (payload != NULL && len > 0)
		memcpy(iov.iov_base + sizeof(struct virtio_console_control),
		     payload, len);

	virtq_add_used_and_signal(vq, idx,
		sizeof(struct virtio_console_control) + len);
}

static int
virtio_console_notify_tx(struct virt_queue *vq)
{
	struct virtio_console *console;
	struct virtio_console_port *port;
	uint16_t idx;
	unsigned int in = 0, out = 0;

	console = virtio_dev_to_console(vq->dev);
	port = virtio_console_vq_to_port(console, vq);

	virtq_disable_notify(vq);

	for (;;) {
		idx = virtq_get_descs(vq, vq->iovec,
				VRING_USED_F_NO_NOTIFY,
				&in, &out);
		if (idx < 0)
			break;

		if (idx == vq->num) {
			if (virtq_enable_notify(vq)) {
				virtq_disable_notify(vq);
				continue;
			}
		}

		if (in) {
			pr_err("unexpected description from guest\n");
			break;
		}

		if (port != NULL)
			port->cb(port, port->arg, vq->iovec, out);

		virtq_add_used_and_signal(vq, idx, 0);
	}

	return 0;
}

static int
virtio_console_notify_rx(struct virt_queue *vq)
{
	struct virtio_console *console;
	struct virtio_console_port *port;

	console = virtio_dev_to_console(vq->dev);
	port = virtio_console_vq_to_port(console, vq);

	if (!port->rx_ready) {
		port->rx_ready = 1;
		virtq_disable_notify(vq);
	}

	return 0;
}

static void
virtio_console_reset_backend(struct virtio_console_backend *be)
{
	if (!be)
		return;

	if (be->fd != STDIN_FILENO)
		mevent_delete_close(be->evp);
	else
		mevent_delete(be->evp);

	if (be->be_type == VIRTIO_CONSOLE_BE_PTY && be->pts_fd > 0) {
		close(be->pts_fd);
		be->pts_fd = -1;
	}

	be->evp = NULL;
	be->fd = -1;
	be->open = false;
}

static void
virtio_console_backend_read(int fd __attribute__((unused)),
			    enum ev_type t __attribute__((unused)),
			    void *arg)
{
	struct virtio_console_port *port;
	struct virtio_console_backend *be = arg;
	struct virt_queue *vq;
	struct iovec iov;
	static char dummybuf[2048];
	int len;
	uint16_t idx;
	unsigned int in, out;

	port = be->port;
	vq = virtio_console_port_to_vq(port, true);

	if (!be->open || !port->rx_ready) {
		len = read(be->fd, dummybuf, sizeof(dummybuf));
		if (len == 0)
			goto close;
		return;
	}

	if (!virtq_has_descs(vq)) {
		len = read(be->fd, dummybuf, sizeof(dummybuf));
		virtq_notify(vq);
		if (len == 0)
			goto close;
		return;
	}

	virtq_disable_notify(vq);

	do {
		idx = virtq_get_descs(vq, &iov, 1, &in, &out);
		len = readv(be->fd, &iov, in);
		if (len <= 0) {
			virtq_discard_desc(vq, 1);
			virtq_notify(vq);

			/* no data available */
			if (len == -1 && errno == EAGAIN) {
				virtq_enable_notify(vq);
				return;
			}

			/* any other errors */
			goto close;
		}

		virtq_add_used_and_signal(vq, idx, len);
	} while (virtq_has_descs(vq));

close:
	virtio_console_reset_backend(be);
	pr_warn("vtcon: be read failed and close! len = %d, errno = %d\n",
		len, errno);
}

static void
virtio_console_backend_write(struct virtio_console_port *port, void *arg,
			     struct iovec *iov, int niov)
{
	struct virtio_console_backend *be;
	int ret;

	be = arg;

	if (be->fd == -1)
		return;

	printf("iov fd-%d 0x%lx 0x%lx niov-%d\n", be->fd, (unsigned long)iov->iov_base,
			iov->iov_len, niov);

	ret = writev(be->fd, iov, niov);
	if (ret <= 0) {
		/* backend cannot receive more data. For example when pts is
		 * not connected to any client, its tty buffer will become full.
		 * In this case we just drop data from guest hvc console.
		 */
		if (ret == -1 && errno == EAGAIN)
			return;

		virtio_console_reset_backend(be);
		pr_warn("vtcon: be write failed! errno = %d\n", errno);
	}
}

static void
virtio_console_restore_stdio(void)
{
	tcsetattr(STDIN_FILENO, TCSANOW, &virtio_console_saved_tio);
	fcntl(STDIN_FILENO, F_SETFL, virtio_console_saved_flags);
	stdio_in_use = false;
}

static bool
virtio_console_backend_can_read(enum virtio_console_be_type be_type)
{
	return (be_type == VIRTIO_CONSOLE_BE_FILE) ? false : true;
}

static int
virtio_console_open_backend(const char *path,
			    enum virtio_console_be_type be_type)
{
	int fd = -1;

	switch (be_type) {
	case VIRTIO_CONSOLE_BE_PTY:
		fd = posix_openpt(O_RDWR | O_NOCTTY);
		if (fd == -1)
			pr_warn("vtcon: posix_openpt failed, errno = %d\n",
				errno);
		else if (grantpt(fd) == -1 || unlockpt(fd) == -1) {
			pr_warn("vtcon: grant/unlock failed, errno = %d\n",
				errno);
			close(fd);
			fd = -1;
		}
		break;
	case VIRTIO_CONSOLE_BE_STDIO:
		if (stdio_in_use) {
			pr_warn("vtcon: stdio is used by other device\n");
			break;
		}
		fd = STDIN_FILENO;
		stdio_in_use = true;
		break;
	case VIRTIO_CONSOLE_BE_TTY:
		fd = open(path, O_RDWR | O_NONBLOCK);
		if (fd < 0)
			pr_warn("vtcon: open failed: %s\n", path);
		else if (!isatty(fd)) {
			pr_warn("vtcon: not a tty: %s\n", path);
			close(fd);
			fd = -1;
		}
		break;
	case VIRTIO_CONSOLE_BE_FILE:
		fd = open(path, O_WRONLY|O_CREAT|O_APPEND|O_NONBLOCK, 0666);
		if (fd < 0)
			pr_warn("vtcon: open failed: %s\n", path);
		break;
	default:
		pr_warn("not supported backend %d!\n", be_type);
	}

	return fd;
}

static int
virtio_console_config_backend(struct virtio_console_backend *be)
{
	int fd, flags;
	char *pts_name = NULL;
	int slave_fd = -1;
	struct termios tio, saved_tio;

	if (!be || be->fd == -1)
		return -1;

	fd = be->fd;
	switch (be->be_type) {
	case VIRTIO_CONSOLE_BE_PTY:
		pts_name = ttyname(fd);
		if (pts_name == NULL) {
			pr_warn("vtcon: ptsname return NULL, errno = %d\n",
				errno);
			return -1;
		}

		slave_fd = open(pts_name, O_RDWR);
		if (slave_fd == -1) {
			pr_warn("vtcon: slave_fd open failed, errno = %d\n",
				errno);
			return -1;
		}

		tcgetattr(slave_fd, &tio);
		cfmakeraw(&tio);
		tcsetattr(slave_fd, TCSAFLUSH, &tio);
		be->pts_fd = slave_fd;

		pr_warn("***********************************************\n");
		pr_warn("virt-console backend redirected to %s\n", pts_name);
		pr_warn("***********************************************\n");

		flags = fcntl(fd, F_GETFL);
		fcntl(fd, F_SETFL, flags | O_NONBLOCK);
		break;
	case VIRTIO_CONSOLE_BE_TTY:
	case VIRTIO_CONSOLE_BE_STDIO:
		tcgetattr(fd, &tio);
		saved_tio = tio;
		cfmakeraw(&tio);
		tio.c_cflag |= CLOCAL;
		tcsetattr(fd, TCSANOW, &tio);

		if (be->be_type == VIRTIO_CONSOLE_BE_STDIO) {
			flags = fcntl(fd, F_GETFL);
			fcntl(fd, F_SETFL, flags | O_NONBLOCK);

			virtio_console_saved_flags = flags;
			virtio_console_saved_tio = saved_tio;
			atexit(virtio_console_restore_stdio);
		}
		break;
	default:
		break; /* nothing to do */
	}

	return 0;
}

static int
virtio_console_add_backend(struct virtio_console *console,
			   const char *name, const char *path,
			   enum virtio_console_be_type be_type,
			   bool is_console)
{
	struct virtio_console_backend *be;
	int error = 0, fd = -1;

	be = calloc(1, sizeof(struct virtio_console_backend));
	if (be == NULL) {
		error = -1;
		goto out;
	}

	fd = virtio_console_open_backend(path, be_type);
	if (fd < 0) {
		error = -1;
		goto out;
	}

	be->fd = fd;
	be->be_type = be_type;

	if (virtio_console_config_backend(be) < 0) {
		pr_warn("vtcon: virtio_console_config_backend failed\n");
		error = -1;
		goto out;
	}

	be->port = virtio_console_add_port(console, name,
		virtio_console_backend_write, be, is_console);
	if (be->port == NULL) {
		pr_warn("vtcon: virtio_console_add_port failed\n");
		error = -1;
		goto out;
	}

	if (virtio_console_backend_can_read(be_type)) {
		if (isatty(fd)) {
			be->evp = mevent_add(fd, EVF_READ,
					virtio_console_backend_read, be);
			if (be->evp == NULL) {
				pr_warn("vtcon: mevent_add failed\n");
				error = -1;
				goto out;
			}
		}
	}

	virtio_console_open_port(be->port, true);
	be->open = true;

out:
	if (error != 0) {
		if (be) {
			if (be->evp)
				mevent_delete(be->evp);
			if (be->port) {
				be->port->enabled = false;
				be->port->arg = NULL;
			}
			if (be->be_type == VIRTIO_CONSOLE_BE_PTY &&
				be->pts_fd > 0)
				close(be->pts_fd);
			free(be);
		}
		if (fd != -1 && fd != STDIN_FILENO)
			close(fd);
	}

	return error;
}

static void
virtio_console_close_backend(struct virtio_console_backend *be)
{
	if (!be)
		return;

	switch (be->be_type) {
	case VIRTIO_CONSOLE_BE_PTY:
		if (be->pts_fd > 0) {
			close(be->pts_fd);
			be->pts_fd = -1;
		}
		break;
	case VIRTIO_CONSOLE_BE_STDIO:
		virtio_console_restore_stdio();
		break;
	default:
		break;
	}

	be->fd = -1;
	be->open = false;
	memset(be->port, 0, sizeof(*be->port));
}

static void
virtio_console_close_all(struct virtio_console *console)
{
	int i;
	struct virtio_console_port *port;
	struct virtio_console_backend *be;

	for (i = 0; i < console->nports; i++) {
		port = &console->ports[i];

		if (!port->enabled)
			continue;

		be = (struct virtio_console_backend *)port->arg;
		if (be) {
			if (be->evp) {
				if (be->fd != STDIN_FILENO)
					mevent_delete_close(be->evp);
				else
					mevent_delete(be->evp);
			}

			virtio_console_close_backend(be);
			free(be);
		}
	}
}

static enum virtio_console_be_type
virtio_console_get_be_type(const char *backend)
{
	int i;

	for (i = 0; i < VIRTIO_CONSOLE_BE_MAX; i++)
		if (strcasecmp(backend, virtio_console_be_table[i]) == 0)
			return i;

	return VIRTIO_CONSOLE_BE_INVALID;
}

static int
virtio_console_init(struct vdev *vdev, char *opts)
{
	struct virtio_console *console;
	char *backend = NULL;
	char *portname = NULL;
	char *portpath = NULL;
	char *opt;
	pthread_mutexattr_t attr;
	enum virtio_console_be_type be_type;
	bool is_console = false;
	int rc;

	if (!opts) {
		pr_warn("vtcon: invalid opts\n");
		return -1;
	}

	console = calloc(1, sizeof(struct virtio_console));
	if (!console) {
		pr_warn("vtcon: calloc returns NULL\n");
		return -1;
	}

	rc = virtio_device_init(&console->virtio_dev, vdev,
				VIRTIO_TYPE_CONSOLE,
				VIRTIO_CONSOLE_MAXQ,
				VIRTQUEUE_MAX_SIZE);
	if (rc) {
		pr_err("failed to init vdev %d\n", rc);
		return rc;
	}

	vdev_set_pdata(vdev, console);
	console->virtio_dev.ops = &vcon_ops;

	/* set the feature of the virtio dev */
	virtio_set_feature(&console->virtio_dev, VIRTIO_CONSOLE_F_SIZE);
	virtio_set_feature(&console->virtio_dev, VIRTIO_F_VERSION_1);
	virtio_set_feature(&console->virtio_dev, VIRTIO_CONSOLE_F_MULTIPORT);

	console->config = (struct virtio_console_config *)
			console->virtio_dev.config;
	console->config->max_nr_ports = VIRTIO_CONSOLE_MAXPORTS;
	console->config->cols = 80;
	console->config->rows = 25;

	/* init mutex attribute properly to avoid deadlock */
	rc = pthread_mutexattr_init(&attr);
	if (rc)
		pr_debug("mutexattr init failed with erro %d!\n", rc);
	rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	if (rc)
		pr_debug("virtio_console: mutexattr_settype failed with "
					"error %d!\n", rc);

	rc = pthread_mutex_init(&console->mtx, &attr);
	if (rc)
		pr_debug("virtio_console: pthread_mutex_init failed with "
					"error %d!\n", rc);

	/* create control port */
	console->control_port.console = console;
	console->control_port.txq = 2;
	console->control_port.rxq = 3;
	console->control_port.cb = virtio_console_control_tx;
	console->control_port.enabled = true;

	/* virtio-console,[@]stdio|tty|pty|file:portname[=portpath]
	 * [,[@]stdio|tty|pty|file:portname[=portpath]]
	 */
	while ((opt = strsep(&opts, ",")) != NULL) {
		backend = strsep(&opt, ":");

		if (backend == NULL) {
			pr_warn("vtcon: no backend is specified!\n");
			return -1;
		}

		if (backend[0] == '@') {
			is_console = true;
			backend++;
		} else
			is_console = false;

		be_type = virtio_console_get_be_type(backend);
		if (be_type == VIRTIO_CONSOLE_BE_INVALID) {
			pr_warn("vtcon: invalid backend %s!\n",
				backend);
			return 0;
		}

		if (opt != NULL) {
			portname = strsep(&opt, "=");
			portpath = opt;
			if (portpath == NULL
				&& be_type != VIRTIO_CONSOLE_BE_STDIO
				&& be_type != VIRTIO_CONSOLE_BE_PTY) {
				pr_warn("vtcon: portpath missing for %s\n",
					portname);
				return -1;
			}

			if (virtio_console_add_backend(console, portname,
				portpath, be_type, is_console) < 0) {
				pr_warn("vtcon: add port failed %s\n",
					portname);
				return -1;
			}
		}
	}

	return 0;
}

static int virtio_console_event(struct vdev *vdev)
{
	struct virtio_console *vcon;

	if (!vdev)
		return -EINVAL;

	vcon = (struct virtio_console *)vdev_get_pdata(vdev);
	if (!vcon)
		return -EINVAL;

	return virtio_handle_event(&vcon->virtio_dev);
}

static void
virtio_console_deinit(struct vdev *dev)
{
	struct virtio_console *console;

	console = (struct virtio_console *)vdev_get_pdata;
	if (console) {
		virtio_console_close_all(console);
		if (console->config)
			free(console->config);
		free(console);
	}
}

struct vdev_ops virtio_console_ops = {
	.name 		= "virtio_console",
	.dev_init	= virtio_console_init,
	.dev_deinit	= virtio_console_deinit,
	.handle_event	= virtio_console_event,
};

DEFINE_VDEV_TYPE(virtio_console_ops);
