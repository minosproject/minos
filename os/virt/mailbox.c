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

#include <minos/minos.h>
#include <virt/mailbox.h>
#include <asm/svccc.h>
#include <virt/hypercall.h>
#include <virt/vmm.h>
#include <virt/vm.h>
#include <minos/sched.h>
#include <virt/virq.h>

#define MAX_MAILBOX_NR	CONFIG_MAX_MAILBOX_NR
#define MAILBOX_MAGIC	0xabcdefeeUL

typedef int (*mailbox_hvc_handler_t)(uint64_t arg0, uint64_t arg1);

static int mailbox_index = 0;
static struct mailbox *mailboxs[MAX_MAILBOX_NR];

static uint32_t inline generate_mailbox_cookie(int o1, int o2, int index)
{
	return (MAILBOX_MAGIC << 32) | (o1 << 16) | (o2 << 8) | index;
}

static void inline exract_mailbox_cookie(uint64_t cookie,
		int *o1, int *o2, int *index, uint32_t *magic)
{
	*o1 = (cookie & 0x0000000000ff0000) >> 16;
	*o1 = (cookie & 0x000000000000ff00) >> 8;
	*index = cookie & 0xff;
	*magic = cookie >> 32;
}

static void mailbox_vm_init(struct mailbox *mb, int event)
{
	int i, j;
	struct vm *vm;
	struct mailbox_vm_info *entry;

	for (i = 0; i < 2; i++) {
		/*
		 * map the shared memory for vm and allocate
		 * the virq for the mailbox
		 */
		entry = &mb->mb_info[i];
		vm = mb->owner[i];
		entry->iomem = vm_map_shmem(vm,
				mb->shmem, mb->shmem_size, VM_IO);
		if (!entry->iomem)
			panic("no enough shared memory vm-%d\n", vm->vmid);

		entry->connect_virq = alloc_vm_virq(vm);
		entry->disconnect_virq = alloc_vm_virq(vm);
		if (!entry->connect_virq || !entry->disconnect_virq)
			panic("no enough virq for vm-%d\n", vm->vmid);

		for (j = 0; j < event; j++) {
			entry->event[j] = alloc_vm_virq(vm);
			if (!entry->event[j])
				panic("no enough virq for vm-%d\n", vm->vmid);
		}
	}
}

struct mailbox *create_mailbox(const char *name,
		int o1, int o2, size_t size, int event)
{
	struct vm *vm1, *vm2;
	struct mailbox *mailbox;

	if (mailbox_index >= MAX_MAILBOX_NR) {
		pr_err("mailbox count beyond the max size\n");
		return NULL;
	}

	vm1 = get_vm_by_id(o1);
	vm2 = get_vm_by_id(o2);
	if (!vm1 || !vm2)
		return NULL;

	mailbox = zalloc(sizeof(struct mailbox));
	if (!mailbox)
		return NULL;

	mailbox->owner[0] = vm1;
	mailbox->owner[1] = vm2;
	mailbox->vm_status[0] = MAILBOX_STATUS_DISCONNECT;
	mailbox->vm_status[1] = MAILBOX_STATUS_DISCONNECT;
	spin_lock_init(&mailbox->lock);
	mailbox->cookie = generate_mailbox_cookie(o1, o2, mailbox_index);
	mailboxs[mailbox_index++] = mailbox;

	if (!size)
		goto out;

	/*
	 * the current memory allocation system has a limitation
	 * that get_io_pages can not get memory which bigger than
	 * 2M. if need to get memory bigger than 2M can use
	 * alloc_mem_block and map these memory to IO memory ?
	 */
	size = PAGE_BALIGN(size);
	mailbox->shmem = get_io_pages(PAGE_NR(size));
	if (!mailbox->shmem)
		panic("no more memory for %s\n", name);

	mailbox_vm_init(mailbox, event);
out:
	return mailbox;
}

static int mailbox_query_instance(uint64_t index, uint64_t arg2)
{
	int i;
	struct vm *vm = get_current_vm();
	struct mailbox *mailbox;
	struct mailbox_instance *is;
	struct mailbox_instance *instance = (struct mailbox_instance *)arg2;

	/*
	 * the mailbox driver in native vm will query all
	 * the mailbox instance for him and create the relate
	 * driver the name usually represent the type of this
	 * mailbox
	 */
	if (index >= MAX_MAILBOX_NR)
		return -EINVAL;

	for (i = index; i < MAX_MAILBOX_NR; i++) {
		mailbox = mailboxs[i];
		if ((vm == mailbox->owner[0]) ||
				(vm == mailbox->owner[1])) {
			is = map_vm_mem((unsigned long)instance, sizeof(*is));
			if (!is)
				panic("invaild vm address %p\n", instance);

			is->cookie = mailbox->cookie;
			strcpy(is->name, mailbox->name);
			is->owner_id = (vm == mailbox->owner[0] ? 0 : 1);
			unmap_vm_mem((unsigned long)instance, sizeof(*is));
			return (index + 1);
		}
	}

	return -ENOENT;
}

static struct mailbox *cookie_to_mailbox(uint64_t cookie)
{
	uint32_t magic;
	struct mailbox *mailbox;
	struct vm *vm = get_current_vm();
	int o1 = VMID_INVALID, o2 = VMID_INVALID, index;

	exract_mailbox_cookie(cookie, &o1, &o2, &index, &magic);
	if ((vm->vmid != o1) && (vm->vmid != o2))
		panic("mailbox is not belong to vm-%d\n", vm->vmid);

	if (magic != MAILBOX_MAGIC)
		panic("invalid mailbox\n");

	if (index >= MAX_MAILBOX_NR)
		panic("invalid mailbox index\n");

	mailbox = mailboxs[index];
	if (!mailbox) {
		pr_err("mailbox-%d is not created\n", index);
		return NULL;
	}

	return mailbox;
}

static inline int mailbox_owner_id(struct mailbox *mailbox, struct vm *vm)
{
	return (vm == mailbox->owner[0] ? 0 : 1);
}

static int mailbox_get_info(uint64_t cookie, uint64_t arg2)
{
	int id;
	struct mailbox_vm_info *inf;
	struct vm *vm = get_current_vm();
	struct mailbox *mailbox = cookie_to_mailbox(cookie);
	struct mailbox_vm_info *info = (struct mailbox_vm_info *)arg2;

	if (!mailbox)
		return -EINVAL;

	inf = map_vm_mem((unsigned long)info, sizeof(*inf));
	if (!inf)
		return -EINVAL;

	id = (vm == mailbox->owner[0] ? 0 : 1);
	memcpy(inf, &mailbox->mb_info[id], sizeof(*info));
	unmap_vm_mem((unsigned long)info, sizeof(*inf));

	return 0;
}

static int mailbox_connect(uint64_t cookie, uint64_t noused)
{
	int id;
	struct mailbox *mb = cookie_to_mailbox(cookie);
	struct vm *vm = get_current_vm();

	if (!mb)
		return -EINVAL;

	id = mailbox_owner_id(mb, vm);
	if (mb->vm_status[id] == MAILBOX_STATUS_CONNECTED) {
		pr_warn("mailbox areadly in connected state %d\n",
				vm->vmid);
		return -EINVAL;
	}

	spin_lock(&mb->lock);
	mb->vm_status[id] = MAILBOX_STATUS_CONNECTED;
	if (mb->vm_status[!id] == MAILBOX_STATUS_CONNECTED) {
		send_virq_to_vm(mb->owner[!id],
				mb->mb_info[!id].connect_virq);
	}
	spin_unlock(&mb->lock);

	return 0;
}

static int mailbox_disconnect(uint64_t cookie, uint64_t noused)
{
	int id;
	struct mailbox *mb = cookie_to_mailbox(cookie);
	struct vm *vm = get_current_vm();

	if (!mb)
		return -EINVAL;

	id = mailbox_owner_id(mb, vm);
	if (mb->vm_status[id] == MAILBOX_STATUS_DISCONNECT) {
		pr_warn("mailbox areadly in disconnected state %d\n",
				vm->vmid);
		return -EINVAL;
	}

	spin_lock(&mb->lock);
	mb->vm_status[id] = MAILBOX_STATUS_DISCONNECT;
	if (mb->vm_status[!id] == MAILBOX_STATUS_CONNECTED) {
		send_virq_to_vm(mb->owner[!id],
				mb->mb_info[!id].disconnect_virq);
	}
	spin_unlock(&mb->lock);

	return 0;
}

static int mailbox_post_event(uint64_t cookie, uint64_t event_id)
{
	int id;
	struct mailbox *mb = cookie_to_mailbox(cookie);
	struct vm *vm = get_current_vm();
	struct mailbox_vm_info *info;

	if (!mb)
		return -EINVAL;

	id = mailbox_owner_id(mb, vm);
	info = &mb->mb_info[id];

	if ((event_id >= MAILBOX_MAX_EVENT) ||
			(info->event[event_id] == 0))
		return -EINVAL;

	send_virq_to_vm(vm, info->event[event_id]);
	return 0;
}

static mailbox_hvc_handler_t mailbox_hvc_handlers[] = {
	mailbox_query_instance,
	mailbox_get_info,
	mailbox_connect,
	mailbox_disconnect,
	mailbox_post_event,
	NULL
};

static int mailbox_hvc_handler(gp_regs *c, uint32_t id, uint64_t *args)
{
	int index, ret;
	mailbox_hvc_handler_t handler;

	if (!vm_is_native(get_current_vm()))
		panic("only native vm can call mailbox hypercall\n");

	index = id - HVC_MAILBOX_FN(0);
	if (index >= sizeof(mailbox_hvc_handlers)) {
		pr_err("unsupport mailbox hypercall %d\n", index);
		HVC_RET1(c, -EINVAL)
	}

	handler = mailbox_hvc_handlers[index];
	ret = handler(args[1], args[2]);
	HVC_RET1(c, ret);
}

DEFINE_HVC_HANDLER("vm_mailbox_handler", HVC_TYPE_HVC_MAILBOX,
		HVC_TYPE_HVC_MAILBOX, mailbox_hvc_handler);
