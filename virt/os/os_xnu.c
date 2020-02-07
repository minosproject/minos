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
#include <virt/virq_chip.h>
#include <virt/virq.h>
#include <virt/vmbox.h>
#include <minos/sched.h>
#include <minos/arch.h>
#include <virt/os.h>
#include <virt/resource.h>
#include <asm/vtimer.h>
#include <asm/virt.h>
#include <asm/processer.h>

#define ASOC_VTIMER_VIRQ	26
#define ASOC_DCZVA_SIZE		0x40

struct virq_chip *create_aic_virqchip(struct vm *vm,
		unsigned long base, unsigned long size);

static int apple_soc_dczva_trap(struct vcpu *vcpu, unsigned long va)
{
	unsigned long pa = guest_va_to_pa(va, 0);

	if (pa > 0x80000000)
		memset((void *)pa, 0, ASOC_DCZVA_SIZE);
	else {
		pa = guest_va_to_pa(va, 1);
		if (pa > 0x80000000)
			memset((void *)pa, 0, ASOC_DCZVA_SIZE);
		else
			pr_err("wrong address va:0x%p pa:0x%p\n", va, pa);
	}

	return 0;
}

static int apple_soc_unknow_sysreg(struct vcpu *vcpu,
		int reg, int read, unsigned long *value)
{
	*value = 0;

	switch (reg) {
	case ESR_SYSREG_ASOC_HID11:
		pr_debug("apple soc reg: %s\n", "ESR_SYSREG_ASOC_HID11\n");
		break;
	case ESR_SYSREG_ASOC_HID5:
		pr_debug("apple soc reg: %s\n", "ESR_SYSREG_ASOC_HID5\n");
		break;
	case ESR_SYSREG_ASOC_HID4:
		pr_debug("apple soc reg: %s\n", "ESR_SYSREG_ASOC_HID4\n");
		break;
	case ESR_SYSREG_ASOC_HID8:
		pr_debug("apple soc reg: %s\n", "ESR_SYSREG_ASOC_HID8\n");
		break;
	case ESR_SYSREG_ASOC_HID7:
		pr_debug("apple soc reg: %s\n", "ESR_SYSREG_ASOC_HID7\n");
		break;
	case ESR_SYSREG_ASOC_LSU_ERR_STS:
		pr_debug("apple soc reg: %s\n", "ESR_SYSREG_ASOC_LSU_ERR_STS\n");
		break;
	case ESR_SYSREG_ASOC_PMC0:
		pr_debug("apple soc reg: %s\n", "ESR_SYSREG_ASOC_PMC0\n");
		break;
	case ESR_SYSREG_ASOC_PMC1:
		pr_debug("apple soc reg: %s\n", "ESR_SYSREG_ASOC_PMC1\n");
		break;
	case ESR_SYSREG_ASOC_PMCR1:
		pr_debug("apple soc reg: %s\n", "ESR_SYSREG_ASOC_PMCR1\n");
		break;
	case ESR_SYSREG_ASOC_PMSR:
		pr_debug("apple soc reg: %s\n", "ESR_SYSREG_ASOC_PMSR\n");
		break;
	default:
		pr_debug("apple soc reg: %s\n", "UNKNOWN\n");
		break;
	}

	return 0;
}

static int xnu_create_gvm_res_apple(struct vm *vm)
{
	struct arm_virt_data *arm_data = vm->arch_data;

	request_virq_pervcpu(vm, ASOC_VTIMER_VIRQ, VIRQF_FIQ);

	vm->virq_chip = create_aic_virqchip(vm, 0x20e100000, 0x100000);
	if (!vm->virq_chip) {
		pr_err("create virq chiq for apple soc failed\n");
		return -EINVAL;
	}

	/*
	 * install trap callback for apple soc
	 */
	arm_data->dczva_trap = apple_soc_dczva_trap;
	arm_data->sysreg_emulation = apple_soc_unknow_sysreg;

	return 0;
}

static void xnu_vm_init(struct vm *vm)
{

}

static void xnu_vcpu_init(struct vcpu *vcpu)
{
	gp_regs *regs;

	/*
	 * xnu will use X0 to store the boot argument
	 * so set the setup data address to x0
	 */
	if (get_vcpu_id(vcpu) == 0) {
		arch_init_vcpu(vcpu, (void *)vcpu->vm->entry_point, NULL);
		regs = (gp_regs *)vcpu->task->stack_base;
		regs->x0 = (uint64_t)vcpu->vm->setup_data;

		vcpu_online(vcpu);
	}

	virq_set_fiq(vcpu, vcpu->vm->vtimer_virq);
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
//	if (vm->flags & VM_FLAGS_XNU_APPLE)
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
