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
#include <minos/virq.h>
#include <minos/of.h>
#include <minos/mm.h>
#include <minos/vcpu.h>
#include <minos/virq_chip.h>
#include <minos/spinlock.h>

static int virqchip_enter_to_guest(void *item, void *data)
{
	unsigned long flags;
	struct vcpu *vcpu = (struct vcpu *)item;
	struct virq_struct *virq_struct = vcpu->virq_struct;
	struct virq_chip *vc = vcpu->vm->virq_chip;

	/*
	 * if there is no pending virq for this vcpu
	 * clear the virq state in HCR_EL2 then just return
	 * else inject the virq
	 */
	spin_lock_irqsave(&virq_struct->lock, flags);

	if (!(vc->flags & VIRQCHIP_F_HW_VIRT)) {
		if (is_list_empty(&virq_struct->pending_list) &&
				is_list_empty(&virq_struct->active_list)) {
			arch_clear_virq_flag();
			goto out;
		} else
			arch_set_virq_flag();
	}

	if (vc->enter_to_guest)
		vc->enter_to_guest(vcpu, vc->inc_pdata);
out:
	spin_unlock_irqrestore(&virq_struct->lock, flags);
	return 0;
}

static int virqchip_exit_from_guest(void *item, void *data)
{
	unsigned long flags;
	struct vcpu *vcpu = (struct vcpu *)item;
	struct virq_struct *virq_struct = vcpu->virq_struct;
	struct virq_chip *vc = vcpu->vm->virq_chip;

	if (vc->exit_from_guest) {
		spin_lock_irqsave(&virq_struct->lock, flags);
		vc->exit_from_guest(vcpu, vc->inc_pdata);
		spin_unlock_irqrestore(&virq_struct->lock, flags);
	}

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

static int virqchip_init(void)
{
	register_hook(virqchip_enter_to_guest,
			MINOS_HOOK_TYPE_ENTER_TO_GUEST);
	register_hook(virqchip_exit_from_guest,
			MINOS_HOOK_TYPE_EXIT_FROM_GUEST);
	return 0;
}
subsys_initcall(virqchip_init);
