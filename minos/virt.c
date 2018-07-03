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
#include <minos/vmodule.h>
#include <minos/sched.h>
#include <minos/arch.h>
#include <minos/virt.h>

extern void virqs_init(void);
extern void parse_memtags(void);
extern void parse_virqs(void);

extern struct virt_config virt_config;
struct virt_config *mv_config = &virt_config;

int taken_from_guest(gp_regs *regs)
{
	return arch_taken_from_guest(regs);
}

void exit_from_guest(struct vcpu *vcpu, gp_regs *regs)
{
	do_hooks(vcpu, (void *)regs, MINOS_HOOK_TYPE_EXIT_FROM_GUEST);
}

void enter_to_guest(struct vcpu *vcpu, gp_regs *regs)
{
	do_hooks(vcpu, (void *)regs, MINOS_HOOK_TYPE_ENTER_TO_GUEST);
}

void save_vcpu_vcpu_state(struct vcpu *vcpu)
{
	save_vcpu_vmodule_state(vcpu);
}

void restore_vcpu_vcpu_state(struct vcpu *vcpu)
{
	restore_vcpu_vmodule_state(vcpu);
}

int virt_init(void)
{
	vmodules_init();

	if (create_vms() == 0)
		return -ENOENT;

	parse_memtags();
	vms_init();
	parse_virqs();
	virqs_init();

	return 0;
}
