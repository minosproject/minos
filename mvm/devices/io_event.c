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
#include <list.h>
#include <io_event.h>
#include <sys/epoll.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>

static int ep_fd;

static LIST_HEAD(io_list);

int io_event_init(void)
{
	ep_fd = epoll_create(MAX_EPOLL_EVENT);

	return ep_fd > 0 ? 0 : ep_fd;
}

void io_event_deinit(void)
{
	struct epoll_event epoll;
	struct io_event *event, *tmp;

	list_for_each_entry_safe(event, tmp, &io_list, list) {
		if (event->type == IO_EVENT_READ)
			epoll.events = EPOLLIN;
		else
			epoll.events = EPOLLOUT;

		epoll.data.ptr = event;
		epoll_ctl(ep_fd, EPOLL_CTL_DEL, event->fd, &epoll);
		close(event->fd);

		list_del(&event->list);
		free(event);
	}

	if (ep_fd > 0)
		close(ep_fd);

	ep_fd = -1;
}

int io_event_add(int fd, int type, void *data,
		void (*callback)(int, int, void *))
{
	int ret;
	struct epoll_event event;
	struct io_event *io_event;

	if (fd < 0 || !callback)
		return -EINVAL;

	io_event = calloc(1, sizeof(struct io_event));
	if (!io_event)
		return -ENOMEM;

	io_event->fd = fd;
	io_event->type = type;
	io_event->callback = callback;
	io_event->data = data;

	if (type == IO_EVENT_READ)
		event.events = EPOLLIN;
	else
		event.events = EPOLLOUT;

	event.data.ptr = io_event;

	ret = epoll_ctl(ep_fd, EPOLL_CTL_ADD, io_event->fd, &event);
	if (ret) {
		pr_err("add epoll event faild\n");
		free(io_event);
		return ret;
	} else
		list_add_tail(&io_list, &io_event->list);

	return 0;
}

static void do_io_event(struct epoll_event *events, int nr)
{
	int i;
	struct io_event *event;

	for (i = 0; i < nr; i++) {
		event = events[i].data.ptr;
		event->callback(event->fd, event->type, event->data);
	}
}

int io_event_loop(void)
{
	int ret;
	struct epoll_event events[MAX_EPOLL_EVENT];

	while (1) {
		ret = epoll_wait(ep_fd, events, MAX_EPOLL_EVENT, -1);
		if (ret < 0) {
			pr_err("io event epoll error\n");
			continue;
		}

		do_io_event(events, ret);
	}

	return -EFAULT;
}
