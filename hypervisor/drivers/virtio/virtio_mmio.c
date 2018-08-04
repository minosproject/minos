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
#include <minos/virtio_mmio.h>
#include <minos/vmm.h>
#include <minos/mm.h>
#include <minos/bitmap.h>

/*
 * the io space for virtio the first section of vm_mmap,
 * and the max size for virtio region
 * is VM_VIRTIO_MEM_SIZE (if the max vm size is 64, the
 * size is 16M), since the mmio space needed all mapped
 * to vm0, so every vm may have 256KB virt_io memory space
 *
 * each virtio device has 0x200 byte memory space, the max
 * count of virtio device is 1024 if there max have 64 vms
 */

/*
 * virtio's io mem in guest-vm is from 0x10000000 - 0x10100000
 * 1M memory space, and each virtio device have 4k io space
 */

#define VIRTIO_IO_OFFSET(addr) (addr - ALIGN(address , PAGE_SIZE))

static DECLARE_BITMAP(virtio_hvmirq_bitmap, VIRTIO_IRQ_COUNT);

static int vaild_virtio_address(unsigned long addr)
{
	if ((addr < VIRTIO_PHY_MEM_BASE) ||
			(addr >= VIRTIO_PHY_MEM_END))
		return 0;

	return 1;
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
	void *iomem = (void *)guest_ipa_to_pa(address);

	switch (offset) {
	case VIRTIO_MMIO_DRIVER_FEATURES:
		break;
	case VIRTIO_MMIO_DRIVER_FEATURES_SEL:
		break;
	case VIRTIO_MMIO_GUEST_PAGE_SIZE:
		break;
	case VIRTIO_MMIO_QUEUE_SEL:
		break;
	case VIRTIO_MMIO_QUEUE_NUM:
		break;
	case VIRTIO_MMIO_QUEUE_ALIGN:
		break;
	case VIRTIO_MMIO_QUEUE_READY:
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
	virq = find_next_zero_bit(virtio_irq_bitmap, VIRTIO_IRQ_COUNT, 0);
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
	int ret, hvm_virq;
	unsigned long vm0_base, vir;
	void *base;

	if (!vaild_virtio_address(vir))
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
	if (create_guest_mapping(vm, gvm_iomem_ipa(vir), base, VM_IO | VM_RO))
		goto free_mem;

	vm0_base = create_hvm_iomem_mmap(vm, base, size);
	if (!vm0_base)
		goto free_mem;

	memset((void *)base, 0, PAGE_SIZE);
	virtio_device_init(vm, base, hvm_virq,
			VIRTIO_GVM_IRQ_START + dtype);

	return vm0_base;

free_mem:
	free_vm_pages(base);
	return NULL;
}

static struct mmio_ops virtio_mmio_ops = {
	.read	= virtio_mmio_read,
	.wrtie	= virtio_mmio_write,
	.check	= vaild_virtio_address,
};

static void virtio_create_vm(void *item, void *arg)
{
	/* to be done */
}

static int virtio_device_init(void)
{
	bitmap_clear(virtio_hvmirq_bitmap, 0, VIRTIO_IRQ_COUNT);
	register_hook(virtio_create_vm, MINOS_HOOK_TYPE_CREATE_VM);

	register_mmio_emulation_handler("virtio", &virtio_mmio_ops);
	return 0;
}

device_initcall(virtio_device_init);
