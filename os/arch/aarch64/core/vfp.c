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

struct vfp_context {
	uint64_t regs[64] __align(16);
	uint32_t fpexc32_el2;
	uint32_t fpsr;
	uint32_t fpcr;
};

static void vfp_state_init(struct vcpu *vcpu, void *context)
{
	memset(context, 0, sizeof(struct vfp_context));
}

static void vfp_state_save(struct vcpu *vcpu, void *context)
{
	struct vfp_context *c = (struct vfp_context *)context;

	if (vm_is_32bit(vcpu->vm))
		c->fpexc32_el2 = read_sysreg32(FPEXC32_EL2);

	c->fpsr = read_sysreg32(FPSR);
	c->fpcr = read_sysreg32(FPCR);

	asm volatile("stp q0, q1, [%1, #16 * 0]\n\t"
		     "stp q2, q3, [%1, #16 * 2]\n\t"
		     "stp q4, q5, [%1, #16 * 4]\n\t"
                     "stp q6, q7, [%1, #16 * 6]\n\t"
                     "stp q8, q9, [%1, #16 * 8]\n\t"
                     "stp q10, q11, [%1, #16 * 10]\n\t"
                     "stp q12, q13, [%1, #16 * 12]\n\t"
                     "stp q14, q15, [%1, #16 * 14]\n\t"
                     "stp q16, q17, [%1, #16 * 16]\n\t"
                     "stp q18, q19, [%1, #16 * 18]\n\t"
                     "stp q20, q21, [%1, #16 * 20]\n\t"
                     "stp q22, q23, [%1, #16 * 22]\n\t"
                     "stp q24, q25, [%1, #16 * 24]\n\t"
                     "stp q26, q27, [%1, #16 * 26]\n\t"
                     "stp q28, q29, [%1, #16 * 28]\n\t"
                     "stp q30, q31, [%1, #16 * 30]\n\t"
                     : "=Q" (*c->regs) : "r" (c->regs));
}

static void vfp_state_restore(struct vcpu *vcpu, void *context)
{
	struct vfp_context *c = (struct vfp_context *)context;

	if (vm_is_32bit(vcpu->vm))
		write_sysreg(c->fpexc32_el2, FPEXC32_EL2);

	write_sysreg(c->fpsr, FPSR);
	write_sysreg(c->fpcr, FPCR);

	asm volatile("ldp q0, q1, [%1, #16 * 0]\n\t"
		     "ldp q2, q3, [%1, #16 * 2]\n\t"
                     "ldp q4, q5, [%1, #16 * 4]\n\t"
                     "ldp q6, q7, [%1, #16 * 6]\n\t"
                     "ldp q8, q9, [%1, #16 * 8]\n\t"
                     "ldp q10, q11, [%1, #16 * 10]\n\t"
                     "ldp q12, q13, [%1, #16 * 12]\n\t"
                     "ldp q14, q15, [%1, #16 * 14]\n\t"
                     "ldp q16, q17, [%1, #16 * 16]\n\t"
                     "ldp q18, q19, [%1, #16 * 18]\n\t"
                     "ldp q20, q21, [%1, #16 * 20]\n\t"
                     "ldp q22, q23, [%1, #16 * 22]\n\t"
                     "ldp q24, q25, [%1, #16 * 24]\n\t"
                     "ldp q26, q27, [%1, #16 * 26]\n\t"
                     "ldp q28, q29, [%1, #16 * 28]\n\t"
                     "ldp q30, q31, [%1, #16 * 30]\n\t"
                     : : "Q" (*c->regs), "r" (c->regs));
}

static int vfp_vmodule_init(struct vmodule *vmodule)
{
	vmodule->context_size	= sizeof(struct vfp_context);
	vmodule->pdata		= NULL;
	vmodule->state_init	= vfp_state_init;
	vmodule->state_save	= vfp_state_save;
	vmodule->state_restore	= vfp_state_restore;

	return 0;
}

MINOS_MODULE_DECLARE(vfp, "vfp", (void *)vfp_vmodule_init);
