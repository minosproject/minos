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
#include <minos/vcpu.h>
#include <minos/virt.h>
#include <minos/vm.h>
#include <minos/vcpu.h>
#include <minos/vmm.h>
#include <minos/os.h>
#include <minos/sched.h>

extern void get_vcpu_affinity(int *aff, int nr);

static inline void vminfo_to_vmtag(struct vm_info *info, struct vmtag *tag)
{
	tag->vmid = VMID_INVALID;
	tag->name = (char *)info->name;
	tag->type = (char *)info->os_type;
	tag->nr_vcpu = info->nr_vcpus;
	tag->entry = info->entry;
	tag->setup_data = info->setup_data;
	tag->bit64 = info->bit64;

	/* for the dynamic need to get the affinity dynamicly */
	get_vcpu_affinity(tag->vcpu_affinity, tag->nr_vcpu);
}

int vm_power_up(int vmid)
{
	struct vm *vm = get_vm_by_id(vmid);

	if (vm == NULL) {
		pr_error("vm-%d is not exist\n", vmid);
		return -ENOENT;
	}

	vm_vcpus_init(vm);
	return 0;
}

int create_new_vm(struct vm_info *info)
{
	int ret;
	struct vm *vm;
	struct vmtag vme;
	size_t size;
	struct vm_info *vm_info = (struct vm_info *)
			guest_va_to_pa((unsigned long)info, 1);

	if (!vm_info)
		return VMID_INVALID;

	/*
	 * first check whether there are enough memory for
	 * this vm and the vm's memory base need to be start
	 * at 0xc0000000 or higher, if the mem_start is 0,
	 * then set it to default 0xc0000000
	 */
	size = vm_info->mem_size;

	if (vm_info->mem_start == 0)
		vm_info->mem_start = GVM_NORMAL_MEM_START;

	if (vm_info->mem_start < GVM_NORMAL_MEM_START)
		return -EINVAL;

	if ((vm_info->mem_start + size) >= GVM_NORMAL_MEM_END)
		return -EINVAL;

	if (!has_enough_memory(size))
		return -ENOMEM;

	if (vm_info->nr_vcpus > NR_CPUS)
		return -EINVAL;

	memset(&vme, 0, sizeof(struct vmtag));
	vminfo_to_vmtag(vm_info, &vme);

	vm = create_dynamic_vm(&vme);
	if (!vm)
		return VMID_INVALID;

	/*
	 * allocate memory to this vm
	 */
	vm->bit64 = !!vm_info->bit64;
	vm_mm_struct_init(vm);

	ret = vm_mmap_init(vm, size);
	if (ret) {
		pr_error("no more mmap space for vm\n");
		goto release_vm;
	}

	vm_info->mmap_base = vm->mm.hvm_mmap_base;

	ret = alloc_vm_memory(vm, vm_info->mem_start, size);
	if (ret)
		goto release_vm;

	dsb();

	return (vm->vmid);

release_vm:
	destroy_vm(vm);

	return -ENOMEM;
}
