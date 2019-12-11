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
#include <virt/vmbox.h>
#include <asm/svccc.h>
#include <virt/hypercall.h>
#include <virt/vmm.h>
#include <virt/vm.h>
#include <minos/sched.h>
#include <virt/virq.h>
#include <libfdt/libfdt.h>
#include <virt/virq_chip.h>
#include <minos/of.h>
#include <asm/io.h>

#define VMBOX_MAX_COUNT	16
#define VMBOX_MAX_VQS	4

#define BE_IDX		0
#define FE_IDX		1

#define VMBOX_IPC_ALL_ENTRY_SIZE	0x100

struct vmbox_info {
	int owner[2];
	uint32_t id[2];
	uint32_t vqs;
	uint32_t vring_num;
	uint32_t vring_size;
	uint32_t shmem_size;
	unsigned long flags;
	char type[32];
};

struct vmbox_hook {
	char name[32];
	struct list_head list;
	struct vmbox_hook_ops *ops;
};

static LIST_HEAD(vmbox_con_list);
static LIST_HEAD(vmbox_hook_list);

struct vring_used_elem {
	uint32_t id;
	uint32_t len;
} __packed__;

struct vring_used {
	uint16_t flags;
	uint16_t idx;
	struct vring_used_elem ring[];
} __packed__;

struct vring_avail {
	uint16_t flags;
	uint16_t idx;
	uint16_t ring[];
} __packed__;

struct vring_desc {
	uint64_t addr;
	uint32_t len;
	uint16_t flags;
	uint16_t next;
} __packed__;

typedef int (*vmbox_hvc_handler_t)(struct vm *vm,
		struct vmbox_device *vmbox, unsigned long arg);

static int vmbox_index = 0;
static struct vmbox *vmboxs[VMBOX_MAX_COUNT];

static inline struct vmbox_hook *vmbox_find_hook(char *name)
{
	struct vmbox_hook *hook;

	list_for_each_entry(hook, &vmbox_hook_list, list) {
		if (strcmp(hook->name, name) == 0)
			return hook;
	}

	return NULL;
}

int register_vmbox_hook(char *name, struct vmbox_hook_ops *ops)
{
	int len;
	struct vmbox_hook *hook;

	if (!ops)
		return -EINVAL;

	hook = vmbox_find_hook(name);
	if (hook) {
		pr_warn("vmbox hook [%s] areadly register\n", name);
		return -EEXIST;
	}

	hook = zalloc(sizeof(*hook));
	if (!hook)
		return -ENOMEM;

	len = strlen(name);
	len = MIN(len, 31);
	strncpy(hook->name, name, len);
	hook->ops = ops;
	list_add_tail(&vmbox_hook_list, &hook->list);

	return 0;
}

static void do_vmbox_hook(struct vmbox *vmbox)
{
	struct vmbox_hook *hook;

	hook = vmbox_find_hook(vmbox->name);
	if (!hook)
		return;

	if (hook->ops->vmbox_init)
		hook->ops->vmbox_init(vmbox);
}

static inline unsigned int
vmbox_virtq_vring_desc_size(unsigned int qsz, unsigned long align)
{
	int desc_size;

	desc_size = sizeof(struct vring_desc) * qsz + (align - 1);
	desc_size &= ~(align - 1);

	return desc_size;
}

static inline unsigned int
vmbox_virtq_vring_avail_size(unsigned int qsz, unsigned long align)
{
	int  avail_size;

	avail_size = sizeof(uint16_t) * (3 + qsz) + (align - 1);
	avail_size &= ~(align - 1);

	return avail_size;

}

static inline unsigned int
vmbox_virtq_vring_used_size(unsigned int qsz, unsigned long align)
{
	int used_size;

	used_size = sizeof(uint16_t) * 2 + sizeof(struct vring_used_elem) *
		(qsz + 1) + (align - 1);
	used_size &= ~(align - 1);

	return used_size;

}

static inline unsigned int
vmbox_virtq_vring_size(unsigned int qsz, unsigned long align)
{
	return vmbox_virtq_vring_desc_size(qsz, align) +
		vmbox_virtq_vring_avail_size(qsz, align) +
		vmbox_virtq_vring_used_size(qsz, align);
}

static inline size_t get_vmbox_iomem_header_size(struct vmbox_info *vinfo)
{
	size_t size = VMBOX_IPC_ALL_ENTRY_SIZE;

	/*
	 * calculate the vring desc size first, each vmbox will
	 * have 0x100 IPC region
	 */
	size += vmbox_virtq_vring_size(vinfo->vring_num,
			VMBOX_VRING_ALGIN_SIZE) * vinfo->vqs;

	return size;
}

static inline size_t get_vmbox_iomem_buf_size(struct vmbox_info *vinfo)
{
	return (vinfo->vring_num * vinfo->vring_size * vinfo->vqs);
}

static inline size_t get_vmbox_iomem_size(struct vmbox_info *vinfo)
{
	return get_vmbox_iomem_header_size(vinfo) +
				get_vmbox_iomem_buf_size(vinfo);
}

static struct vmbox_device *create_vmbox_device(struct vmbox *vmbox, int idx)
{
	struct vmbox_device *vdev;

	vdev = zalloc(sizeof(*vdev));
	if (!vdev)
		return NULL;

	spin_lock_init(&vdev->lock);
	vdev->vmbox= vmbox;
	vdev->is_backend = (idx == BE_IDX);
	vdev->vm = get_vm_by_id(vmbox->owner[idx]);

	return vdev;
}

static int create_vmbox_devices(struct vmbox *vmbox)
{
	struct vmbox_device *vdev_be;
	struct vmbox_device *vdev_fe;

	vdev_be = create_vmbox_device(vmbox, BE_IDX);
	vdev_fe = create_vmbox_device(vmbox, FE_IDX);

	if (!vdev_be || !vdev_fe)
		goto release_vdev;

	/* connect to each other */
	vdev_be->bro = vdev_fe;
	vdev_fe->bro = vdev_be;
	vmbox->devices[BE_IDX] = vdev_be;
	vmbox->devices[FE_IDX] = vdev_fe;

	return 0;

release_vdev:
	if (vdev_be)
		free(vdev_be);
	if (vdev_fe)
		free(vdev_fe);
	return -ENOMEM;
}

static int create_vmbox(struct vmbox_info *vinfo)
{
	struct vm *vm1, *vm2;
	struct vmbox *vmbox;
	size_t iomem_size = 0;
	int o1 = vinfo->owner[BE_IDX];
	int o2 = vinfo->owner[FE_IDX];

	vm1 = get_vm_by_id(o1);
	vm2 = get_vm_by_id(o2);
	if (!vm1 || !vm2) {
		pr_warn("no such VM %d %d\n", o1, o2);
		return -ENOENT;
	}

	vmbox = zalloc(sizeof(*vmbox));
	if (!vmbox)
		return -ENOMEM;

	vmbox->owner[BE_IDX] = o1;
	vmbox->owner[FE_IDX] = o2;
	memcpy(vmbox->devid, vinfo->id, sizeof(uint32_t) * 2);
	strcpy(vmbox->name, vinfo->type);
	vmbox->vqs = vinfo->vqs;
	vmbox->vring_num = vinfo->vring_num;
	vmbox->vring_size = vinfo->vring_size;
	vmbox->id = vmbox_index;
	vmbox->flags = vinfo->flags;
	vmboxs[vmbox_index++] = vmbox;

	/*
	 * the current memory allocation system has a limitation
	 * that get_io_pages can not get memory which bigger than
	 * 2M. if need to get memory bigger than 2M can use
	 * alloc_mem_block and map these memory to IO memory ?
	 *
	 * if the vmbox use fix shared memory size, the shmem_size
	 * will be set before this fucntion, otherwise it means
	 * the vmbox is use virtq mode
	 */
	if (!vinfo->shmem_size) {
		iomem_size = get_vmbox_iomem_size(vinfo);
		iomem_size = PAGE_BALIGN(iomem_size);
	} else {
		iomem_size = PAGE_BALIGN(vinfo->shmem_size);
	}

	vmbox->shmem = get_io_pages(PAGE_NR(iomem_size));
	if (!vmbox->shmem)
		panic("no more memory for %s\n", vinfo->type);

	/* init all the header memory to zero */
	if (!vinfo->shmem_size)
		memset(vmbox->shmem, 0, get_vmbox_iomem_header_size(vinfo));
	else
		memset(vmbox->shmem, 0, VMBOX_IPC_ALL_ENTRY_SIZE);

	vmbox->shmem_size = iomem_size;

	if (create_vmbox_devices(vmbox))
		pr_err("create vmbox device for %s failed\n", vmbox->name);

	do_vmbox_hook(vmbox);

	return 0;
}

int of_create_vmbox(struct device_node *node)
{
	int ret;
	struct vmbox_info vinfo;

	if (vmbox_index >= VMBOX_MAX_COUNT) {
		pr_err("vmbox count beyond the max size\n");
		return -ENOSPC;
	}

	memset(&vinfo, 0, sizeof(vinfo));

	/*
	 * vmbox-id	     - id of this vmbox dev_id and vendor_id
	 * vmbox-type	     - the type of this vmbox
	 * vmbox-owner	     - the owner of this vmbox, two owmer
	 * vmbox-vqs         - how many virtqueue for this vmbox
	 * vmbox-vrings      - how many vrings for each virtqueue
	 * vmbox-vring-size  - buffer size of each vring
	 * vmbox-shmem-size  - do not using virtq to transfer data between VM
	 */
	if (of_get_u32_array(node, "vmbox-owner", (uint32_t *)vinfo.owner, 2) < 2)
		return -EINVAL;

	of_get_string(node, "vmbox-type", vinfo.type, sizeof(vinfo.type) - 1);
	if (of_get_u32_array(node, "vmbox-id", vinfo.id, 2) <= 0)
		pr_warn("unknown vmbox id for %s\n", vinfo.type);

	if (of_get_bool(node, "platform-device"))
		vinfo.flags |= VMBOX_F_PLATFORM_DEV;

	ret = of_get_u32_array(node, "vmbox-shmem-size", &vinfo.shmem_size, 1);
	if (ret && vinfo.shmem_size > 0)
		goto out;

	if (of_get_u32_array(node, "vmbox-vqs", &vinfo.vqs, 1) <= 0)
		return -EINVAL;
	of_get_u32_array(node, "vmbox-vrings", &vinfo.vring_num, 1);
	of_get_u32_array(node, "vmbox-vring-size", &vinfo.vring_size, 1);

out:
	return create_vmbox(&vinfo);
}

struct vmbox_controller *vmbox_get_controller(struct vm *vm)
{
	struct vmbox_controller *vc;

	list_for_each_entry(vc, &vmbox_con_list, list) {
		if (vc->vm == vm)
			return vc;
	}

	return NULL;
}

static int vmbox_device_attach(struct vmbox *vmbox, struct vmbox_device *vdev)
{
	struct vmm_area *va;
	struct vm *vm = vdev->vm;
	struct vmbox_controller *_vc;

	/*
	 * find the real vmbox which this vmbox device
	 * should connected to
	 */
	_vc = vmbox_get_controller(vm);
	if (!_vc) {
		pr_err("can not find vmbox_controller for vmbox dev\n");
		return -ENOENT;
	}

	vdev->vc = _vc;
	vdev->vring_virq = alloc_vm_virq(vm);
	vdev->ipc_virq = alloc_vm_virq(vm);
	if (!vdev->vring_virq || !vdev->ipc_virq)
		return -ENOSPC;

	/* platform device has already alloc virtual memory */
	if (!vdev->iomem) {
		va = alloc_free_vmm_area(&vm->mm, vmbox->shmem_size,
				PAGE_MASK, VM_MAP_P2P);
		if (!va)
			return -ENOMEM;
		vdev->iomem = va->start;
		vdev->iomem_size = vmbox->shmem_size;
		map_vmm_area(&vm->mm, va, (unsigned long)vmbox->shmem);
	}

	vdev->devid = _vc->dev_cnt++;
	_vc->devices[vdev->devid] = vdev;
	wmb();

	/*
	 * set the device online for the vm
	 * if the controller of this vmbox_device is areadly
	 * online then send a virq to the VM, or if there
	 * are pending virqs for this controller, send a virq
	 * too
	 */
	 _vc->dev_state |= (1 << vdev->devid);
	 _vc->irq_state |= VMBOX_CON_INT_TYPE_DEV_ONLINE;

	if (_vc->status)
		send_virq_to_vm(_vc->vm, _vc->virq);

	return 0;
}

static void vmbox_con_online(struct vmbox_controller *vc)
{
	int i;
	struct vmbox *vmbox;
	struct vm *vm = vc->vm;

	pr_notice("vmbox controller for vm%d is online\n", vm->vmid);

	for (i = 0; i < vmbox_index; i++) {
		vmbox = vmboxs[i];

		/*
		 * when a vmbox controller is online, we first check all
		 * the clinet device which is attached to this device and
		 * report the client device to the VM. VM then load the
		 * driver for this client device.
		 *
		 * once the client device finish to setup, it will write
		 * it's status to power on, at this time, get the host
		 * device which the client device connected to, and
		 * report the host device to the VM
		 */
		if ((vmbox->owner[BE_IDX] == vm->vmid)) {
			if (vmbox_device_attach(vmbox, vmbox->devices[BE_IDX]))
				pr_err("vmbox device attached failed\n");
		}
	}

	/*
	 * if there are devices online on this controller, and
	 * there is pending virqs, send the virq to the VM
	 */
	if (vc->irq_state)
		send_virq_to_vm(vc->vm, vc->virq);
}

static int vmbox_handle_con_read(struct vmbox_controller *vc,
		unsigned long offset, unsigned long *value)
{
	switch (offset) {
	case VMBOX_CON_DEV_STAT:
		*value = vc->dev_state;
		break;
	case VMBOX_CON_INT_STATUS:
		/* once read, clear the status */
		*value = vc->irq_state;
		vc->irq_state = 0;
		break;
	default:
		*value = 0;
		break;
	}

	return 0;
}

static int inline __vmbox_handle_dev_read(struct vmbox_device *vdev,
		int reg, unsigned long *v)
{
	unsigned long flags;
	struct vmbox *vmbox = vdev->vmbox;

	switch (reg) {
	case VMBOX_DEV_IPC_TYPE:
		/* read and clear */
		spin_lock_irqsave(&vdev->lock, flags);
		*v = vdev->ipc_type;
		vdev->ipc_type = 0;
		spin_unlock_irqrestore(&vdev->lock, flags);
		break;

	case VMBOX_DEV_DEVICE_ID:
		if (vdev->is_backend)
			*v = vmbox->devid[0];
		else
			*v = vmbox->devid[0] + 1;
		break;

	case VMBOX_DEV_VQS:
		*v = vmbox->vqs;
		break;

	case VMBOX_DEV_VRING_NUM:
		*v = vmbox->vring_num;
		break;

	case VMBOX_DEV_VRING_SIZE:
		*v = vmbox->vring_size;
		break;

	case VMBOX_DEV_VRING_BASE_HI:
		*v = (unsigned long)vdev->iomem >> 32;
		break;

	case VMBOX_DEV_VRING_BASE_LOW:
		*v = (unsigned long)vdev->iomem & 0xffffffff;
		break;

	case VMBOX_DEV_MEM_SIZE:
		*v = vdev->iomem_size;
		break;

	case VMBOX_DEV_ID:
		*v = vdev->devid | VMBOX_DEVICE_MAGIC;
		break;

	case VMBOX_DEV_VENDOR_ID:
		*v = vmbox->devid[1];
		break;

	case VMBOX_DEV_VRING_IRQ:
		*v = vdev->vring_virq;
		break;

	case VMBOX_DEV_IPC_IRQ:
		*v = vdev->ipc_virq;
		break;

	default:
		*v = 0;
		break;
	}

	return 0;
}

static int vmbox_handle_dev_read(struct vmbox_controller *vc,
		unsigned long offset, unsigned long *value)
{
	int devid;
	uint32_t reg;
	struct vmbox_device *vdev;

	offset -= VMBOX_CON_DEV_BASE;
	devid = offset / VMBOX_CON_DEV_SIZE;
	reg = offset % VMBOX_CON_DEV_SIZE;

	if (devid >= sizeof(vc->devices)) {
		pr_err("vmbox devid invaild %d\n");
		return -EINVAL;
	}

	vdev = vc->devices[devid];
	if (!vdev) {
		pr_err("no such device %d\n", devid);
		return -ENOENT;
	}

	return __vmbox_handle_dev_read(vdev, reg, value);
}

static int vmbox_con_read(struct vdev *vdev, gp_regs *regs,
		unsigned long address, unsigned long *value)
{
	struct vmbox_controller *vc = vdev_to_vmbox_con(vdev);
	unsigned long offset = address - (unsigned long)vc->va;

	if (offset < VMBOX_CON_DEV_BASE)
		return vmbox_handle_con_read(vc, offset, value);
	else
		return vmbox_handle_dev_read(vc, offset, value);
}

static int vmbox_handle_con_write(struct vmbox_controller *vc,
		unsigned long offset, unsigned long *value)
{
	switch (offset) {
	case VMBOX_CON_ONLINE:
		vc->status = 1;
		vmbox_con_online(vc);
		break;
	default:
		break;
	}

	return 0;
}

static int vmbox_handle_dev_write(struct vmbox_controller *vc,
		unsigned long offset, unsigned long *value)
{
	int devid, need_notify = 0;
	uint32_t reg;
	unsigned long flags;
	struct vmbox_device *vdev, *bro;

	offset -= VMBOX_CON_DEV_BASE;
	devid = offset / VMBOX_CON_DEV_SIZE;
	reg = offset % VMBOX_CON_DEV_SIZE;

	if (devid >= sizeof(vc->devices)) {
		pr_err("vmbox devid invaild %d\n");
		return -EINVAL;
	}

	vdev = vc->devices[devid];
	if (!vdev) {
		pr_err("no such device %d\n", devid);
		return -ENOENT;
	}
	bro = vdev->bro;

	switch (reg) {
	case VMBOX_DEV_VRING_EVENT:
		if (!bro->state)
			return 0;

		send_virq_to_vm(vdev->bro->vm, vdev->bro->vring_virq);
		break;
	case VMBOX_DEV_IPC_EVENT:
		/*
		 * write the event_reg and send a virq to the vm
		 * wait last event finised
		 */
		if (!bro->state || (*value >= VMBOX_DEV_IPC_COUNT))
			return 0;

		spin_lock_irqsave(&bro->lock, flags);
		if (!(bro->ipc_type & (1 << *value))) {
			bro->ipc_type |= (1 << *value);
			need_notify = 1;
		}
		spin_unlock_irqrestore(&bro->lock, flags);

		if (need_notify)
			send_virq_to_vm(vdev->bro->vm, vdev->bro->ipc_virq);
		break;
	case VMBOX_DEV_VDEV_ONLINE:
		/*
		 * when vmbox driver write this register, it means that
		 * this device is ok to receive irq or handle the related
		 * event
		 *
		 * for backend, it will invoke the frontend device, for
		 * frontend device, it just update the related status and
		 * then send a virq to backend
		 */
		vdev->state = VMBOX_DEV_STAT_ONLINE;
		pr_notice("vmbox device %s online for vm%d\n",
				vdev->vmbox->name, vdev->vm->vmid);

		if (vdev->is_backend)
			vmbox_device_attach(vdev->vmbox, vdev->bro);
		break;
	default:
		pr_err("unsupport reg 0x%x\n", reg);
		break;
	}

	return 0;
}

static int vmbox_con_write(struct vdev *vdev, gp_regs *regs,
		unsigned long address, unsigned long *value)
{
	struct vmbox_controller *vc = vdev_to_vmbox_con(vdev);
	unsigned long offset = address - (unsigned long)vc->va;

	if (offset < VMBOX_CON_DEV_BASE)
		return vmbox_handle_con_write(vc, offset, value);
	else
		return vmbox_handle_dev_write(vc, offset, value);
}

static void vmbox_con_deinit(struct vdev *vdev)
{
	/* will never called */
}

static void vmbox_con_reset(struct vdev *vdev)
{
	/* will never called */
}

static int __of_setup_vmbox_iomem
(void *dtb, int node, unsigned long iomem, size_t iomem_size)
{
	uint32_t tmp[4];
	uint32_t *args = tmp;
	int size = 0, size_cells, addr_cells;

	size_cells = fdt_size_cells(dtb, 0);
	addr_cells = fdt_address_cells(dtb, 0);

	if (addr_cells == 1) {
		*args++ = cpu_to_fdt32(iomem);
		size++;
	} else {
		*args++ = cpu_to_fdt32(iomem >> 32);
		*args++ = cpu_to_fdt32(iomem & 0xffffffff);
		size += 2;
	}

	if (size_cells == 1) {
		*args++ = cpu_to_fdt32(iomem_size);
		size++;
	} else {
		*args++ = cpu_to_fdt32(iomem_size >> 32);
		*args++ = cpu_to_fdt32(iomem_size & 0xffffffff);
		size += 2;
	}

	fdt_setprop(dtb, node, "reg", (void *)tmp, size * 4);
	return 0;
}

static int __of_setup_vmbox_con_virqs(struct vmbox_controller *vcon,
		void *dtb, int node)
{
	int size = 0;
	uint32_t tmp[10];
	struct vm *vm = vcon->vm;
	struct virq_chip *vc = vm->virq_chip;

	if (!vc->generate_virq) {
		pr_err("no generate_virq in virq_chip\n");
		return -ENOENT;
	}

	size += vc->generate_virq(tmp + size, vcon->virq);
	fdt_setprop(dtb, node, "interrupts", (void *)tmp, size * 4);

	return 0;
}

static void add_vmbox_con_to_vm(struct vm *vm, struct vmbox_controller *vc)
{
	int node;
	char node_name[128];
	void *dtb = vm->setup_data;

	memset(node_name, 0, 128);
	sprintf(node_name, "vmbox-controller@%x", vc->va);

	node = fdt_add_subnode(dtb, 0, node_name);
	if (node < 0) {
		pr_err("failed to add vmbox device %s for vm-%d\n",
				node_name, vm->vmid);
		return;
	}

	fdt_setprop(dtb, node, "compatible", "minos,vmbox", 12);

	__of_setup_vmbox_iomem(dtb, node, (unsigned long)vc->va, PAGE_SIZE);
	__of_setup_vmbox_con_virqs(vc, dtb, node);
}

static int vm_create_vmbox_controller(struct vm *vm)
{
	struct vmbox_controller *vc;
	struct vmm_area *va;

	vc = zalloc(sizeof(*vc));
	if (!vc)
		return -ENOMEM;

	va = alloc_free_vmm_area(&vm->mm, PAGE_SIZE, PAGE_MASK,
			VM_MAP_PT | VM_IO | VM_RO);
	if (!va) {
		free(vc);
		return -ENOMEM;
	}

	vc->virq = alloc_vm_virq(vm);
	if (!vc->virq) {
		free(vc);
		return -ENOENT;
	}

	vc->va = (void *)va->start;
	vc->vm = vm;

	/*
	 * register the vmbox controller for the vm, all the
	 * operation (read/write) on the virtual address will
	 * be trap to hypervisor
	 */
	host_vdev_init(vm, &vc->vdev, (unsigned long)vc->va, PAGE_SIZE);
	vc->vdev.read = vmbox_con_read;
	vc->vdev.write = vmbox_con_write;
	vc->vdev.deinit = vmbox_con_deinit;
	vc->vdev.reset = vmbox_con_reset;

	list_add_tail(&vmbox_con_list, &vc->list);

	add_vmbox_con_to_vm(vm, vc);

	return 0;
}

int vmbox_register_platdev(struct vmbox_device *vdev, void *dtb, char *type)
{
	int node;
	char node_name[128];

	memset(node_name, 0, 128);
	sprintf(node_name, "vmbox-%s@%x", type, vdev->iomem);

	node = fdt_add_subnode(dtb, 0, node_name);
	if (node < 0) {
		pr_err("failed to add platform device %s\n", type);
		return -ENOENT;
	}

	fdt_setprop(dtb, node, "compatible", type, strlen(type) + 1);
	__of_setup_vmbox_iomem(dtb, node, (unsigned long)vdev->iomem,
			vdev->iomem_size);
	return 0;
}

static int vmbox_device_do_hooks(struct vm *vm)
{
	int i;
	struct vmbox_device *vdev;
	struct vmbox_hook *hook;
	struct vmbox *vmbox;

	for (i = 0; i < vmbox_index; i++) {
		vmbox = vmboxs[i];
		hook = vmbox_find_hook(vmbox->name);
		if (!hook)
			continue;

		if (hook->ops->vmbox_be_init &&
				(vmbox->owner[0] == vm->vmid)) {
			vdev = vmbox->devices[0];
			hook->ops->vmbox_be_init(vdev->vm, vmbox, vdev);
		}

		if (hook->ops->vmbox_fe_init &&
				(vmbox->owner[1] == vm->vmid)) {
			vdev = vmbox->devices[1];
			hook->ops->vmbox_fe_init(vdev->vm, vmbox, vdev);
		}
	}

	return 0;
}

int of_setup_vm_vmbox(struct vm *vm)
{
	int ret;

	ret = vm_create_vmbox_controller(vm);
	if (ret)
		return ret;

	return vmbox_device_do_hooks(vm);
}
