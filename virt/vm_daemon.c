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
#include <minos/sched.h>
#include <minos/task.h>
#include <minos/queue.h>
#include <virt/vm.h>
#include <virt/vdev.h>
#include <virt/virq.h>

#define VM_SIGNAL_QUEUE_SIZE 8

static queue_t vm_request_queue;

#define VM_ACTION_NULL 0x0
#define VM_REBOOT 0x03
#define VM_SHUTDOWN 0X4

struct vm_request {
	int action;
	struct vm *vm;
};

static int send_vm_request(struct vm *vm, int req)
{
	struct vm_request *vr;;

	vr = malloc(sizeof(struct vm_request));
	if (!vr)
		return -ENOMEM;

	vr->action = req;
	vr->vm = vm;

	return queue_post(&vm_request_queue, vr);
}

int send_vm_shutdown_request(struct vm *vm)
{
	return send_vm_request(vm, VM_SHUTDOWN);
}

int send_vm_reboot_request(struct vm *vm)
{
	return send_vm_request(vm, VM_REBOOT);
}

static int wait_vcpu_in_state(struct vcpu *vcpu, int state, uint32_t timeout)
{
	int i;

	timeout = (timeout == 0) ? -1 : timeout;
	pr_notice("wait 0x%x ms for vcpu in state %d\n", timeout, state);

	/*
	 * if the target vcpu is on the same physical pcpu, need
	 * call sched() to let it has time to run.
	 */
	for (i = 0; i < timeout / 10; i++) {
		if (vcpu->task->state == state)
			return 0;

		msleep(10);
	}

	return -ETIMEDOUT;
}

static void handle_vm_reboot(struct vm *vm)
{
	struct vcpu *vcpu;
	struct vdev *vdev;

	vm_for_each_vcpu(vm, vcpu) {
		if (wait_vcpu_in_state(vcpu, TASK_STATE_SUSPEND, 1000)) {
			pr_err("vm-%d vcpu-%d power off failed\n",
					vm->vmid, vcpu->vcpu_id);
			return;
		}
	}

	vm_for_each_vcpu(vm, vcpu)
		vcpu_reset(vcpu);

	/* reset the vdev for this vm */
	list_for_each_entry(vdev, &vm->vdev_list, list) {
		if (vdev->reset)
			vdev->reset(vdev);
	}

	vm_virq_reset(vm);
}

static void handle_vm_shutdown(struct vm *vm)
{
	struct vcpu *vcpu;

	vm_for_each_vcpu(vm, vcpu) {
		if (wait_vcpu_in_state(vcpu, TASK_STATE_STOP, 1000)) {
			pr_err("wait %s vcpu%d stop failed\n",
					vm->name, vcpu->vcpu_id);
			return;
		}
	}
}

static void handle_vm_request(struct vm_request *vs)
{
	switch (vs->action) {
	case VM_REBOOT:
		handle_vm_reboot(vs->vm);
		break;
	case VM_SHUTDOWN:
		handle_vm_shutdown(vs->vm);
		break;
	default:
		pr_err("unsupport vm request %d\n", vs->action);
		break;
	}
}

int vm_daemon_main(void *data)
{
	struct vm_request *vs;

	pr_notice("start VM daemon\n");

	for (;;) {
		vs = queue_pend(&vm_request_queue, -1);
		if (!vs) {
			pr_err("something is wrong to receive vm request\n");
			continue;
		}

		handle_vm_request(vs);
		free(vs);
	}
}

void vm_daemon_init(void)
{
	queue_init(&vm_request_queue, VM_SIGNAL_QUEUE_SIZE, NULL);

	if (!create_task("vm-daemon", vm_daemon_main,
				0x2000, OS_PRIO_SYSTEM, -1, 0, NULL))
		pr_err("create vm-daemon task failed\n");
}
