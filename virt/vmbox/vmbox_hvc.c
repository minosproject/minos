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
#include <virt/vmm.h>
#include <virt/vm.h>
#include <minos/sched.h>
#include <virt/virq.h>
#include <libfdt/libfdt.h>
#include <virt/virq_chip.h>
#include <minos/of.h>
#include <asm/io.h>
#include <virt/vmbox.h>

#define BUF_0_SIZE	4096
#define BUF_1_SIZE	2048

/*
 * at least 8K size for the transfer buffer, and
 * in or out buffer size need 2^ align
 */
struct hvc_ring {
	volatile uint32_t ridx;
	volatile uint32_t widx;
	uint32_t size;
	char buf[0];
};

static int hvc_be_init(struct vm *vm, struct vmbox *vmbox,
		struct vmbox_device *vdev)
{
	void *dtb = vm->setup_data;

	/*
	 * only linux system need do this
	 */
	if (!(vmbox->flags & VMBOX_F_PLATFORM_DEV))
		return 0;

	pr_notice("register hvc platform device for vm%d\n", vm->vmid);

	return vmbox_register_platdev(vdev, dtb, "minos,hvc-be");
}

static int hvc_vmbox_init(struct vmbox *vmbox)
{
	void *base = vmbox->shmem + VMBOX_IPC_ALL_ENTRY_SIZE;
	int header_size = sizeof(struct hvc_ring);
	struct hvc_ring *ring;

	ring = (struct hvc_ring *)(base);
	ring->ridx = 0;
	ring->widx = 0;
	ring->size = BUF_0_SIZE;

	ring = (struct hvc_ring *)(base + header_size + BUF_0_SIZE);
	ring->ridx = 0;
	ring->widx = 0;
	ring->size = BUF_1_SIZE;

	return 0;
}

static struct vmbox_hook_ops hvc_ops = {
	.vmbox_be_init = hvc_be_init,
	.vmbox_init = hvc_vmbox_init,
};

static int vmbox_hvc_init(void)
{
	return register_vmbox_hook("hvc", &hvc_ops);
}
subsys_initcall(vmbox_hvc_init);
