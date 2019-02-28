#ifndef __MINOS_MAILBOX_H__
#define __MINOS_MAILBOX_H__

#define MAILBOX_MAX_EVENT		4

#define MAILBOX_EVENT_RING_ID		0

#define MAILBOX_STATUS_DISCONNECT 	0
#define MAILBOX_STATUS_CONNECTED	1

struct mailbox_instance {
	uint64_t cookie;
	char name[32];
	int owner_id;
};

struct mailbox_vm_info {
	uint32_t connect_virq;
	uint32_t disconnect_virq;
	uint32_t ring_event_virq;
	uint32_t event[MAILBOX_MAX_EVENT];
	size_t iomem_size;
	void *iomem;
};

/*
 * owner : owner[0] is the server vm and the
 *         owner[1] is the client vm
 *
 */
struct mailbox {
	char name[32];
	uint64_t cookie;
	int vm_status[2];
	struct vm *owner[2];
	spinlock_t lock;
	void *shmem;
	size_t shmem_size;
	struct mailbox_vm_info mb_info[2];
};

struct mailbox *create_mailbox(const char *name,
		int o1, int o2, size_t size, int event);

#endif
