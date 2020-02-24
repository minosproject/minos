/*
 * BSD 3-Clause License
 *
 * Copyright (C) 2018 Min Le (lemin9538@gmail.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <pthread.h>
#include <mvm.h>
#include <mvm_queue.h>

int mvm_queue_init(struct mvm_queue *queue)
{
	pthread_mutex_init(&queue->mutex, NULL);
	pthread_cond_init(&queue->cond, NULL);

	pthread_mutex_lock(&queue->mutex);
	queue->head = NULL;
	queue->tail = NULL;
	queue->count = 0;
	queue->flags |= QUEUE_START;
	pthread_mutex_unlock(&queue->mutex);

	return 0;
}

int mvm_queue_reset(struct mvm_queue *queue)
{
	struct mvm_node *c, *n;

	pthread_mutex_lock(&queue->mutex);

	queue->flags &= ~QUEUE_START;
	pthread_cond_signal(&queue->cond);

	c = queue->head;
	while (c) {
		n = c->next;
		c->server_index = c->client_index = 0;
		c = n;
	}

	queue->head = queue->tail = NULL;
	queue->count = 0;

	pthread_mutex_unlock(&queue->mutex);

	return 0;
}

int mvm_queue_deinit(struct mvm_queue *queue)
{
	mvm_queue_reset(queue);
	pthread_cond_destroy(&queue->cond);
	pthread_mutex_destroy(&queue->mutex);

	return 0;
}

int mvm_queue_push_node(struct mvm_queue *queue,
		struct mvm_node *node)
{
	if (!node)
		return -EINVAL;

	node->next = NULL;
	node->client_index++;
	pthread_mutex_lock(&queue->mutex);

	if (!queue->tail)
		queue->head = node;
	else
		queue->tail->next = node;
	queue->tail = node;
	queue->count++;

	pthread_cond_signal(&queue->cond);
	pthread_mutex_unlock(&queue->mutex);

	return 0;
}

int mvm_queue_push(struct mvm_queue *queue, uint32_t type,
		void *data, uint32_t size)
{
	struct mvm_node *node;

	node = calloc(1, sizeof(struct mvm_node));
	if (!node) {
		pr_err("failed to get memory for mvm node\n");
		return -ENOMEM;
	}

	node->type = type;
	node->next = NULL;

	if (size > 0) {
		node->data = malloc(size);
		if (!node->data) {
			pr_err("no memory for mvm node data\n");
			free(node);
			return -ENOMEM;
		}

		memcpy(node->data, data, size);
		node->size = size;
	} else {
		node->data = NULL;
		node->size = 0;
	}

	return mvm_queue_push_node(queue, node);
}

struct mvm_node *mvm_queue_pop(struct mvm_queue *queue)
{
	struct mvm_node *node = NULL;

	pthread_mutex_lock(&queue->mutex);

	for(;;) {
		if (!(queue->flags & QUEUE_START)) {
			node = NULL;
			break;
		}

		node = queue->head;
		if (node) {
			queue->head = node->next;
			if (!queue->head)
				queue->tail = NULL;
			queue->count--;
			break;
		} else
			pthread_cond_wait(&queue->cond, &queue->mutex);
	}

	pthread_mutex_unlock(&queue->mutex);

	return node;
}

void mvm_queue_free(struct mvm_node *node)
{
	if (node->flags & NODE_STATIC) {
		node->server_index++;
	} else {
		if (node->data) {
			free(node->data);
			node->data = NULL;
		}

		free(node);
	}
}
