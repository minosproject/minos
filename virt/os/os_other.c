/*
 * Copyright (C) 2019 Min Le (lemin9538@gmail.com)
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
#include <minos/init.h>
#include <virt/vm.h>
#include <virt/resource.h>
#include <virt/os.h>
#include <virt/vmbox.h>

static void default_vcpu_init(struct vcpu *vcpu)
{
	if (get_vcpu_id(vcpu) == 0) {
		arch_init_vcpu(vcpu, (void *)vcpu->vm->entry_point, NULL);
		vcpu_online(vcpu);
	}
}

static void default_vcpu_power_on(struct vcpu *vcpu, unsigned long entry)
{

}

static void default_vm_setup(struct vm *vm)
{
	vmbox_init(vm);
}

static int default_create_guest_vm_resource(struct vm *vm)
{
	return 0;
}

static int default_create_native_vm_resource(struct vm *vm)
{
	return create_native_vm_resource_common(vm);
}

struct os_ops default_os_ops = {
	.vcpu_init	= default_vcpu_init,
	.vcpu_power_on	= default_vcpu_power_on,
	.vm_setup	= default_vm_setup,
	.create_gvm_res = default_create_guest_vm_resource,
	.create_nvm_res = default_create_native_vm_resource,
};

static int os_default_init(void)
{
	return register_os("default-os", OS_TYPE_OTHERS, &default_os_ops);
}
module_initcall(os_default_init);
