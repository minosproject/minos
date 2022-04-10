/*
 * Copyright (C) 2018 - 2019 Min Le (lemin9538@gmail.com)
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
#include <minos/irq.h>
#include <minos/of.h>
#include <minos/mm.h>
#include <minos/spinlock.h>
#include <virt/vm.h>
#include <virt/virq.h>
#include <virt/virq_chip.h>

static int virqchip_enter_to_guest(void *item, void *data)
{
	struct vcpu *vcpu = (struct vcpu *)item;
	struct virq_chip *vc = vcpu->vm->virq_chip;
	int flags;

	if (!vc)
		return -ENOENT;

	/*
	 * if the flags is 0, then means there is no irq inject
	 * to the vcpu, if there are FIQs inject, the bit 31 will
	 * set, and other bit indicate how many irq has been injected.
	 */
	flags = vc->enter_to_guest(vcpu, vc->inc_pdata);
	if (flags == 0) {
		if (!(vc->flags & VIRQCHIP_F_HW_VIRT))
			arch_clear_virq_flag();
	} else {
		if (!(vc->flags & VIRQCHIP_F_HW_VIRT)) {
			if (flags & FIQ_HAS_INJECT)
				arch_set_vfiq_flag();
			else
				arch_set_virq_flag();
		}
	}

	return 0;
}

static int virqchip_exit_from_guest(void *item, void *data)
{
	struct vcpu *vcpu = (struct vcpu *)item;
	struct virq_chip *vc = vcpu->vm->virq_chip;

	if (vc && vc->exit_from_guest)
		return vc->exit_from_guest(vcpu, vc->inc_pdata);
	else
		return 0;
}

struct virq_chip *alloc_virq_chip(void)
{
	struct virq_chip *vchip;

	vchip = zalloc(sizeof(struct virq_chip));
	if (!vchip)
		return NULL;

	return vchip;
}

void virqchip_update_virq(struct vcpu *vcpu,
		struct virq_desc *virq, int action)
{
	struct virq_chip *vc = vcpu->vm->virq_chip;

	if (vc && vc->update_virq)
		vc->update_virq(vcpu, virq, action);
}

void virqchip_send_virq(struct vcpu *vcpu, struct virq_desc *virq)
{
	struct virq_chip *vc = vcpu->vm->virq_chip;

	if (vc && vc->send_virq)
		vc->send_virq(vcpu, virq);
}

int virqchip_get_virq_state(struct vcpu *vcpu, struct virq_desc *virq)
{
	struct virq_chip *vc = vcpu->vm->virq_chip;

	if (vc && vc->get_virq_state)
		return vc->get_virq_state(vcpu, virq);

	return 0;
}

static int __init_text virqchip_init(void)
{
	register_hook(virqchip_enter_to_guest, OS_HOOK_ENTER_TO_GUEST);
	register_hook(virqchip_exit_from_guest, OS_HOOK_EXIT_FROM_GUEST);

	return 0;
}
subsys_initcall(virqchip_init);
