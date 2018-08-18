#ifndef __MVM_IO_EVENT_H__
#define __MVM_IO_EVENT_H__

#define IO_EVENT_READ	(0x1)
#define IO_EVENT_WRITE	(0x2)

#define MAX_EPOLL_EVENT	(32)

struct io_event {
	int fd;
	void (*callback)(int fd, int type, void *data);
	void *data;
	int type;
	struct list_head list;
};

void io_event_deinit(void);
int io_event_add(int fd, int type, void *data,
		void (*callback)(int, int, void *));

int io_event_loop(void);
int io_event_init(void);

#endif
