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
#include <virt/vmodule.h>
#include <asm/vtimer.h>
#include <minos/io.h>
#include <virt/mmio.h>
#include <asm/processer.h>
#include <asm/exception.h>
#include <minos/timer.h>
#include <minos/irq.h>
#include <minos/sched.h>

int vtimer_vmodule_id = INVAILD_MODULE_ID;

static void phys_timer_expire_function(unsigned long data)
{
	struct vtimer *vtimer = (struct vtimer *)data;

	vtimer->cnt_ctl |= CNT_CTL_ISTATUS;

	if (!(vtimer->cnt_ctl & CNT_CTL_IMASK))
		send_virq_to_vcpu(vtimer->vcpu, vtimer->virq);
}

static void virt_timer_expire_function(unsigned long data)
{
	struct vtimer *vtimer = (struct vtimer *)data;

	vtimer->cnt_ctl |= CNT_CTL_IMASK;
	send_virq_to_vcpu(vtimer->vcpu, vtimer->virq);
}

static void vtimer_create_vm(struct vm *vm)
{
	vm->time_offset = NOW();
}

static void vtimer_state_restore(struct vcpu *vcpu, void *context)
{
	struct vtimer_context *c = (struct vtimer_context *)context;
	struct vtimer *vtimer = &c->virt_timer;

	del_timer(&vtimer->timer);

	write_sysreg64(c->offset, CNTVOFF_EL2);
	write_sysreg64(vtimer->cnt_cval, CNTV_CVAL_EL0);
	write_sysreg32(vtimer->cnt_ctl, CNTV_CTL_EL0);
	dsb();
}

static void vtimer_state_save(struct vcpu *vcpu, void *context)
{
	struct vtimer_context *c = (struct vtimer_context *)context;
	struct vtimer *vtimer = &c->virt_timer;

	dsb();
	vtimer->cnt_ctl = read_sysreg32(CNTV_CTL_EL0);
	write_sysreg32(vtimer->cnt_ctl & ~CNT_CTL_ENABLE, CNTV_CTL_EL0);
	vtimer->cnt_cval = read_sysreg64(CNTV_CVAL_EL0);

	if ((vtimer->cnt_ctl & CNT_CTL_ENABLE) &&
		!(vtimer->cnt_ctl & CNT_CTL_IMASK)) {
		mod_timer(&vtimer->timer, ticks_to_ns(vtimer->cnt_cval +
				c->offset - boot_tick));
	}

	dsb();
}

static void vtimer_state_init(struct vcpu *vcpu, void *context)
{
	struct vtimer *vtimer;
	struct vtimer_context *c = (struct vtimer_context *)context;

	c->offset = vcpu->vm->time_offset;

	vtimer = &c->virt_timer;
	vtimer->vcpu = vcpu;
	init_timer_on_cpu(&vtimer->timer, vcpu_affinity(vcpu));
	vtimer->timer.function = virt_timer_expire_function;
	vtimer->timer.data = (unsigned long)vtimer;
	vtimer->virq = 27;
	vtimer->cnt_ctl = 0;
	vtimer->cnt_cval = 0;

	vtimer = &c->phy_timer;
	vtimer->vcpu = vcpu;
	init_timer_on_cpu(&vtimer->timer, vcpu_affinity(vcpu));
	vtimer->timer.function = phys_timer_expire_function;
	vtimer->timer.data = (unsigned long)vtimer;
	vtimer->virq = 30;
	vtimer->cnt_ctl = 0;
	vtimer->cnt_cval = 0;
}

static void vtimer_handle_cntp_ctl(gp_regs *regs, int read, int rn)
{
	struct vtimer *vtimer;
	uint32_t value;
	struct vcpu *vcpu = current_vcpu;
	struct vtimer_context *c = (struct vtimer_context *)
		get_vmodule_data_by_id(vcpu, vtimer_vmodule_id);

	vtimer = &c->phy_timer;

	if (read) {
		set_reg_value(regs, rn, vtimer->cnt_ctl);
	} else {
		value = (uint32_t)get_reg_value(regs, rn);
		value &= ~CNT_CTL_ISTATUS;

		if (value & CNT_CTL_ENABLE)
			value |= vtimer->cnt_ctl & CNT_CTL_ISTATUS;
		vtimer->cnt_ctl = value;

		if (vtimer->cnt_ctl & CNT_CTL_ENABLE) {
			mod_timer(&vtimer->timer, vtimer->cnt_cval + c->offset);
		} else {
			del_timer(&vtimer->timer);
		}
	}
}

static void vtimer_handle_cntp_tval(gp_regs *regs, int read, int rn)
{
	struct vtimer *vtimer;
	unsigned long now;
	unsigned long ticks;
	struct vcpu *vcpu = current_vcpu;
	struct vtimer_context *c = (struct vtimer_context *)
		get_vmodule_data_by_id(vcpu, vtimer_vmodule_id);

	vtimer = &c->phy_timer;
	now = NOW() - c->offset;

	if (read) {
		ticks = ns_to_ticks(vtimer->cnt_cval - now) & 0xffffffff;
		set_reg_value(regs, rn, ticks);
	} else {
		unsigned long value = get_reg_value(regs, rn);

		vtimer->cnt_cval = now + ticks_to_ns(value);
		if (vtimer->cnt_ctl & CNT_CTL_ENABLE) {
			vtimer->cnt_ctl &= ~CNT_CTL_ISTATUS;
			mod_timer(&vtimer->timer, vtimer->cnt_cval + c->offset);
		}
	}
}

static void vtimer_handle_cntp_cval(gp_regs *regs, int read, int rn)
{
	struct vtimer *vtimer;
	struct vcpu *vcpu = current_vcpu;
	struct vtimer_context *c = (struct vtimer_context *)
		get_vmodule_data_by_id(vcpu, vtimer_vmodule_id);
	uint32_t value;

	vtimer = &c->phy_timer;

	if (read) {
		set_reg_value(regs, rn, ns_to_ticks(vtimer->cnt_cval));
	} else {
		value = (uint32_t)get_reg_value(regs, rn);
		vtimer->cnt_cval = ticks_to_ns(value);
		if (vtimer->cnt_ctl & CNT_CTL_ENABLE) {
			vtimer->cnt_ctl &= ~CNT_CTL_ISTATUS;
			mod_timer(&vtimer->timer, vtimer->cnt_cval + c->offset);
		}
	}
}

int vtimer_sysreg_simulation(gp_regs *regs, uint32_t esr_value)
{
	struct esr_sysreg *sysreg = (struct esr_sysreg *)&esr_value;
	uint32_t reg = esr_value & ESR_SYSREG_REGS_MASK;

	switch (reg) {
	case ESR_SYSREG_CNTP_CTL_EL0:
		vtimer_handle_cntp_ctl(regs, sysreg->read, sysreg->reg);
		break;
	case ESR_SYSREG_CNTP_CVAL_EL0:
		vtimer_handle_cntp_cval(regs, sysreg->read, sysreg->reg);
		break;
	case ESR_SYSREG_CNTP_TVAL_EL0:
		vtimer_handle_cntp_tval(regs, sysreg->read, sysreg->reg);
		break;
	default:
		break;
	}

	return 0;
}

static int vtimer_vmodule_init(struct vmodule *vmodule)
{
	vmodule->context_size = sizeof(struct vtimer_context);
	vmodule->pdata = NULL;
	vmodule->state_init = vtimer_state_init;
	vmodule->state_save = vtimer_state_save;
	vmodule->state_restore = vtimer_state_restore;
	vmodule->create_vm = vtimer_create_vm;
	vtimer_vmodule_id = vmodule->id;

	return 0;
}

MINOS_MODULE_DECLARE(armv8_vtimer, "armv8-vtimer",
		(void *)vtimer_vmodule_init);
