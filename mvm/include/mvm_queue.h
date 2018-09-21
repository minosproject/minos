#ifndef __MVM_QUEUE_H__
#define __MVM_QUEUE_H__

#include <inttypes.h>

#define QUEUE_START		(1 << 0)

#define NODE_STATIC		(1 << 0)

struct mvm_node {
	uint32_t flags;
	uint32_t type;
	uint64_t client_index;
	uint64_t server_index;
	uint32_t size;
	void *data;
	struct mvm_node *next;
};

struct mvm_queue {
	uint64_t count;
	uint32_t flags;
	struct mvm_node *head;
	struct mvm_node *tail;
	pthread_cond_t cond;
	pthread_mutex_t mutex;
};

void mvm_queue_free(struct mvm_node *node);
struct mvm_node *mvm_queue_pop(struct mvm_queue *queue);
int mvm_queue_deinit(struct mvm_queue *queue);
int mvm_queue_reset(struct mvm_queue *queue);
int mvm_queue_init(struct mvm_queue *queue);

int mvm_queue_push(struct mvm_queue *queue, uint32_t type,
		void *data, uint32_t size);

int mvm_queue_push_node(struct mvm_queue *queue,
		struct mvm_node *node);

static inline void mvm_queue_wait(struct mvm_node *node)
{
	if (node->flags & NODE_STATIC)
		while (node->server_index < node->client_index);
}

#endif
