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
#include <minos/virq.h>
#include <minos/of.h>
#include <minos/virq_chip.h>

/*
 * The following cases are considered software programming
 * errors and result in UNPREDICTABLE behavior:
 *
 * • Having a List register entry with ICH_LR<n>_EL2.HW= 1
 *   which is associated with a physical interrupt, inactive
 *   state or in pending state in the List registers if the
 *   Distributor does not have the corresponding physical
 *   interrupt in either the active state or the active and
 *   pending state.
 * • If ICC_CTLR_EL1.EOImode == 0 or ICC_CTLR_EL3.EOImode_EL3 == 0
 *   then either:
 *   — Having an active interrupt in the List registers with a priorit
 *   that is not set in the corresponding Active Priorities Register.
 *   — Having two interrupts in the List registers in the active stat
 *   with the same preemption priority.>
 * • Having two or more interrupts with the same pINTID in the Lis
 *   registers for a single virtual CPU interface.
 */
int vgic_irq_enter_to_guest(struct vcpu *vcpu, void *data)
{
	/*
	 * here we send the real virq to the vcpu
	 * before it enter to guest
	 */
	int id = 0;
	struct virq_desc *virq, *n;
	struct virq_chip *vc = vcpu->vm->virq_chip;
	struct virq_struct *virq_struct = vcpu->virq_struct;

	list_for_each_entry_safe(virq, n, &virq_struct->pending_list, list) {
		if (!virq_is_pending(virq)) {
			pr_err("virq is not request %d %d\n", virq->vno, virq->id);
			virq->state = 0;
			if (virq->id != VIRQ_INVALID_ID)
				virqchip_update_virq(vcpu, virq, VIRQ_ACTION_CLEAR);
			list_del(&virq->list);
			virq->list.next = NULL;
			continue;
		}

#if 0
		/*
		 * virq is not enabled this time, need to
		 * send it later, but this will infence the idle
		 * condition jugement TBD
		 */
		if (!virq_is_enabled(virq))
			continue;
#endif

		if (virq->id != VIRQ_INVALID_ID)
			goto __do_send_virq;

		/* allocate a id for the virq */
		id = find_next_zero_bit(vc->irq_bitmap, vc->nr_lrs, id);
		if (id == vc->nr_lrs) {
			pr_warn("virq id is full can not send all virq\n");
			break;
		}

		virq->id = id;
		set_bit(id, vc->irq_bitmap);

__do_send_virq:
		virqchip_send_virq(vcpu, virq);
		virq->state = VIRQ_STATE_PENDING;
		virq_clear_pending(virq);
		dsb();
		list_del(&virq->list);
		list_add_tail(&virq_struct->active_list, &virq->list);
	}

	return 0;
}

int vgic_irq_exit_from_guest(struct vcpu *vcpu, void *data)
{
	/*
	 * here we update the states of the irq state
	 * which the vcpu is handles, since this is running
	 * on percpu and hanlde per_vcpu's data so do not
	 * need spinlock
	 */
	int status;
	struct virq_desc *virq, *n;
	struct virq_chip *vc = vcpu->vm->virq_chip;
	struct virq_struct *virq_struct = vcpu->virq_struct;

	list_for_each_entry_safe(virq, n, &virq_struct->active_list, list) {

		status = virqchip_get_virq_state(vcpu, virq);

		/*
		 * the virq has been handled by the VCPU, if
		 * the virq is not pending again, delete it
		 * otherwise add the virq to the pending list
		 * again
		 */
		if (status == VIRQ_STATE_INACTIVE) {
			if (!virq_is_pending(virq)) {
				virqchip_update_virq(vcpu, virq, VIRQ_ACTION_CLEAR);
				clear_bit(virq->id, vc->irq_bitmap);
				virq->state = VIRQ_STATE_INACTIVE;
				list_del(&virq->list);
				virq->id = VIRQ_INVALID_ID;
				virq->list.next = NULL;
				virq_struct->active_count--;
			} else {
				virqchip_update_virq(vcpu, virq, VIRQ_ACTION_CLEAR);
				list_del(&virq->list);
				list_add_tail(&virq_struct->pending_list, &virq->list);
			}
		} else
			virq->state = status;
	}

	return 0;
}

int gic_vm0_virq_data(uint32_t *array, int vspi_nr, int type)
{
	int i, size = 0;

	if (type & VM_FLAGS_SETUP_OF) {
		for (i = 0; i < vspi_nr; i++) {
			*array++ = cpu_to_of32(0);
			*array++ = cpu_to_of32(i);
			*array++ = cpu_to_of32(4);
			size += (3 * sizeof(uint32_t));
		}
	}

	return size;

}
