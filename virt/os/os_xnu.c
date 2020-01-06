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
#include <minos/mm.h>
#include <virt/vmm.h>
#include <libfdt/libfdt.h>
#include <virt/vm.h>
#include <minos/platform.h>
#include <minos/of.h>
#include <config/config.h>
#include <virt/virq_chip.h>
#include <virt/virq.h>
#include <virt/vmbox.h>
#include <minos/sched.h>
#include <minos/arch.h>
#include <virt/os.h>
#include <virt/resource.h>

static int xnu_create_gvm_res_apple(struct vm *vm)
{

}

static void xnu_vm_init(struct vm *vm)
{

}

static void xnu_vcpu_init(struct vcpu *vcpu)
{

}

static void xnu_vcpu_power_on(struct vcpu *vcpu, unsigned long entry)
{

}

static void xnu_vm_setup(struct vm *vm)
{

}

static int xnu_create_gvm_res(struct vm *vm)
{
	/*
	 * if for apple soc currently only support few soc
	 * apple use AIC and it's own timer controller
	 */
	if (vm->flags & VM_FLAGS_XNU_APPLE)
		xnu_create_gvm_res_apple(vm);

	return 0;
}

static int xnu_create_nvm_res(struct vm *vm)
{
	return create_native_vm_resource_common(vm);
}

static struct os_ops os_xnu_ops = {
	.vm_init	= xnu_vm_init,
	.vcpu_init	= xnu_vcpu_init,
	.vcpu_power_on	= xnu_vcpu_power_on,
	.vm_setup	= xnu_vm_setup,
	.create_gvm_res	= xnu_create_gvm_res,
	.create_nvm_res = xnu_create_nvm_res,
};

static int os_xnu_init(void)
{
	return register_os("xnu", OS_TYPE_XNU, &os_xnu_ops);
}
module_initcall(os_xnu_init);
