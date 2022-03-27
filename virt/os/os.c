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
#include <virt/os.h>
#include <virt/vm.h>

static struct os *oses[OS_TYPE_MAX];

int register_os(char *name, int type, struct os_ops *ops)
{
	struct os *os;

	if (!ops || (oses[type] != NULL)) {
		pr_err("os [%d] error already register or no ops\n", type);
		return -EEXIST;
	}

	os = (struct os *)zalloc(sizeof(struct os));
	if (!os)
		return -ENOMEM;

	os->type = type;
	os->ops = ops;
	strncpy(os->name, name, sizeof(os->name) - 1);
	oses[type] = os;

	return 0;
}

struct os *get_vm_os(char *type)
{
	int i;
	struct os *os;

	if (!type)
		goto out;

	for (i = 0; i < OS_TYPE_MAX; i++) {
		os = oses[i];

		if (!strcmp(os->name, type))
			return os;
	}

out:
	return oses[OS_TYPE_OTHERS];
}

void os_setup_vm(struct vm *vm)
{
	if (vm->os->ops->vm_setup)
		vm->os->ops->vm_setup(vm);
}

int os_create_native_vm_resource(struct vm *vm)
{
	if (vm->os->ops->create_nvm_res)
		return vm->os->ops->create_nvm_res(vm);

	return 0;
}

int os_create_guest_vm_resource(struct vm *vm)
{
	if (vm->os->ops->create_gvm_res)
		return vm->os->ops->create_gvm_res(vm);

	return 0;
}

void os_vcpu_power_on(struct vcpu *vcpu, unsigned long entry)
{
	struct os *os = vcpu->vm->os;

	if (os->ops->vcpu_power_on)
		os->ops->vcpu_power_on(vcpu, entry);
}

void os_vm_init(struct vm *vm)
{
	if (vm->os->ops->vm_init)
		vm->os->ops->vm_init(vm);
}
