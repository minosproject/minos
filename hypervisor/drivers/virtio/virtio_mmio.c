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
#include <minos/vmm.h>
#include <minos/mm.h>
#include <minos/bitmap.h>
#include <minos/virtio.h>
#include <minos/virtio_mmio.h>
#include <minos/mmio.h>
#include <minos/io.h>
#include <minos/sched.h>

/*
 * each virtio device has 0x200 byte memory space, the max
 * count of virtio device is 1024 if there max have 64 vms
 */

#define VIRTIO_IO_OFFSET(addr) (addr - ALIGN(address , PAGE_SIZE))

static DECLARE_BITMAP(virtio_hvmirq_bitmap, VIRTIO_IRQ_COUNT);

static inline void *virtio_device_address(unsigned long address)
{
	struct vm *vm = get_vm_by_id(get_vmid(current_vcpu));

	return vm->virtio_iomem_table[(address -
			VIRTIO_PHY_MEM_BASE) >> PAGE_SHIFT];
}

static int vaild_virtio_address(gp_regs *regs, unsigned long addr)
{
	return !((addr < VIRTIO_PHY_MEM_BASE) ||
			(addr >= VIRTIO_PHY_MEM_END));
}

static int virtio_mmio_read(gp_regs *regs, unsigned long address,
		unsigned long *read_value)
{
	return 0;
}

static int virtio_mmio_write(gp_regs *regs, unsigned long address,
		unsigned long *write_value)
{
	unsigned long offset = VIRTIO_IO_OFFSET(address);
	void *iomem = virtio_device_address(address);

	switch (offset) {
	case VIRTIO_MMIO_HOST_FEATURES:
		break;
	case VIRTIO_MMIO_HOST_FEATURES_SEL:
		break;
	case VIRTIO_MMIO_GUEST_PAGE_SIZE:
		break;
	case VIRTIO_MMIO_QUEUE_SEL:
		break;
	case VIRTIO_MMIO_QUEUE_NUM:
		break;
	case VIRTIO_MMIO_QUEUE_ALIGN:
		break;
	case VIRTIO_MMIO_QUEUE_PFN:
		break;
	case VIRTIO_MMIO_QUEUE_NOTIFY:
		break;
	case VIRTIO_MMIO_INTERRUPT_ACK:
		break;
	case VIRTIO_MMIO_STATUS:
		break;
	case VIRTIO_MMIO_QUEUE_DESC_LOW:
		break;
	case VIRTIO_MMIO_QUEUE_AVAIL_LOW:
		break;
	case VIRTIO_MMIO_QUEUE_USED_LOW:
		break;
	default:
		break;
	}

	return 0;
}

static int alloc_virtio_hvm_virq(void)
{
	int virq = -1;

	preempt_disable();
	virq = find_next_zero_bit(virtio_hvmirq_bitmap, VIRTIO_IRQ_COUNT, 0);
	if (virq >= VIRTIO_IRQ_COUNT)
		virq = -1;
	preempt_enable();

	return virq;
}

static int virtio_device_init(struct vm *vm, void *base, int hi, int gi)
{
	iowrite32(base + VIRTIO_MMIO_VMINFO, vm->vmid);
	iowrite32(base + VIRTIO_MMIO_HVM_IRQ, hi);
	iowrite32(base + VIRTIO_MMIO_GVM_IRQ, gi);

	return 0;
}

void *create_virtio_device(struct vm *vm, int dtype)
{
	int hvm_virq;
	void *vm0_base;
	unsigned long vir;
	void *base;

	if (!vm)
		return NULL;

	if ((dtype > 18) || (dtype == 0) ||
			((dtype > 9) && (dtype < 18))) {
		pr_error("invaild virtio device type %d\n", dtype);
		return NULL;
	}

	vir = VIRTIO_PHY_MEM_BASE + PAGE_SIZE * dtype;

	/* get the hvm irq for this vm */
	hvm_virq = alloc_virtio_hvm_virq();
	if (hvm_virq < 0)
		return NULL;

	/* first alloc IO pages for the devices 4K align*/
	base = vm_alloc_pages(vm, 1);
	if (!base)
		return NULL;

	/*
	 * virtio's io memory need to mapped to host vm mem space
	 * then the backend driver can read/write the io memory
	 * by the way, this memory also need to mapped to the
	 * guest vm 's memory space
	 */
	if (create_guest_mapping(vm, vir, (unsigned long)base,
			PAGE_SIZE, VM_IO | VM_RO))
		return NULL;

	/*
	 * the memory will free when the vm is destoried
	 */
	vm0_base = create_hvm_iomem_mmap((unsigned long)base, PAGE_SIZE);
	if (!vm0_base)
		return NULL;

	memset((void *)base, 0, PAGE_SIZE);
	virtio_device_init(vm, base, hvm_virq,
			VIRTIO_GVM_IRQ_START + dtype);
	vm->virtio_iomem_table[dtype] = base;

	return vm0_base;
}

static struct mmio_ops virtio_mmio_ops = {
	.read	= virtio_mmio_read,
	.write	= virtio_mmio_write,
	.check	= vaild_virtio_address,
};

static void virtio_create_vm(void *item, void *arg)
{
	struct vm *vm = (struct vm *)item;

	vm->virtio_iomem_table = malloc(sizeof(void *) * VIRTIO_DEV_TYPE_MAX);
	if (!vm->virtio_iomem_table)
		pr_error("no memory for vm virito iomem table\n");

	memset(vm->virtio_iomem_table, 0,
			sizeof(void *) * VIRTIO_DEV_TYPE_MAX);
}

static int virtio_init(void)
{
	bitmap_clear(virtio_hvmirq_bitmap, 0, VIRTIO_IRQ_COUNT);
	register_hook(virtio_create_vm, MINOS_HOOK_TYPE_CREATE_VM);

	register_mmio_emulation_handler("virtio", &virtio_mmio_ops);

	return 0;
}

device_initcall(virtio_init);
