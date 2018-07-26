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
#include <minos/arch.h>
#include <minos/virt.h>
#include <minos/os.h>
#include <minos/init.h>

static void linux_vcpu_init(struct vcpu *vcpu)
{
	gp_regs *regs = (gp_regs *)vcpu->stack_base;

	/* fill the dtb address to x0 */
	if (get_vcpu_id(vcpu) == 0) {
		regs->x0 = vcpu->vm->setup_data;
		vcpu_online(vcpu);
	}
}

static void linux_vcpu_power_on(struct vcpu *vcpu, unsigned long entry)
{
	gp_regs *regs = (gp_regs *)vcpu->stack_base;

	regs->elr_elx = entry;
	regs->x0 = 0;
	regs->x1 = 0;
	regs->x2 = 0;
	regs->x3 = 0;
	vcpu_online(vcpu);
}

struct os_ops linux_os_ops = {
	.vcpu_init = linux_vcpu_init,
	.vcpu_power_on = linux_vcpu_power_on,
};

static int os_linux_init(void)
{
	struct os *os;

	os = alloc_os("linux");
	if (!os)
		return -EINVAL;

	os->ops = &linux_os_ops;

	return register_os(os);
}

module_initcall(os_linux_init);
