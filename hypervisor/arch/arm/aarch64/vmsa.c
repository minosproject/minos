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
#include <minos/mmu.h>
#include <config/config.h>
#include <minos/vcpu.h>
#include <minos/vmodule.h>
#include <asm/arch.h>
#include <minos/vmm.h>

/*
 * a - Non-shareable
 * 	This represents memory accessible only by a single processor or other
 * 	agent, so memory accesses never need to be synchronized with other
 * 	processors. This domain is not typically used in SMP systems.
 * b - Inner shareable
 * 	This represents a shareability domain that can be shared by multiple
 * 	processors, but not necessarily all of the agents in the system. A
 * 	system might have multiple Inner Shareable domains. An operation that
 * 	affects one Inner Shareable domain does not affect other Inner Shareable
 * 	domains in the system. An example of such a domain might be a quad-core
 * 	Cortex-A57 cluster.
 * c - Outer shareable
 * 	An outer shareable (OSH) domain re-orderis shared by multiple agents
 * 	and can consist of one or more inner shareable domains. An operation
 * 	that affects an outer shareable domain also implicitly affects all inner
 * 	shareable domains inside it. However, it does not otherwise behave as an
 * 	inner shareable operation.
 * d - Full system
 * 	An operation on the full system (SY) affects all observers in the system.
 */

struct vmsa_context {
	uint64_t vtcr_el2;
	uint64_t ttbr0_el1;
	uint64_t ttbr1_el1;
	uint64_t vttbr_el2;
	uint64_t mair_el1;
	uint64_t amair_el1;
	uint64_t tcr_el1;
	uint64_t par_el1;
}__align(sizeof(unsigned long));

struct aa64mmfr0 {
	uint64_t pa_range : 4;
	uint64_t asid : 4;
	uint64_t big_end : 4;
	uint64_t sns_mem : 4;
	uint64_t big_end_el0 : 4;
	uint64_t t_gran_16k : 4;
	uint64_t t_gran_64k : 4;
	uint64_t t_gran_4k : 4;
	uint64_t res : 32;
};

static inline uint64_t generate_vtcr_el2(void)
{
	uint64_t value = 0;

	/*
	 * vtcr_el2 used to defined the memory attribute
	 * for the EL1, this is defined by hypervisor
	 * and may do not related to physical information
	 */
	value |= (24 << 0);	// t0sz = 0x10 40bits vaddr
	value |= (0x01 << 6);	// SL0: 4kb start at level1
	value |= (0x1 << 8);	// Normal memory, Inner WBWA
	value |= (0x1 << 10);	// Normal memory, Outer WBWA
	value |= (0x3 << 12);	// Inner Shareable

	// TG0 4K
	value |= (0x0 << 14);

	// PS --- pysical size 1TB
	value |= (2 << 16);

	// vmid -- 8bit
	value |= (0x0 << 19);

	return value;
}

static uint64_t generate_vttbr_el2(uint32_t vmid, unsigned long base)
{
	uint64_t value = 0;

	value = base ;
	value |= (uint64_t)vmid << 48;

	return value;
}

int el2_stage2_init(void)
{
	/*
	 * now just support arm fvp, TBD to support more
	 * platform
	 */
	uint64_t value, dcache, icache;
	struct aa64mmfr0 aa64mmfr0;

	value = read_id_aa64mmfr0_el1();
	memcpy(&aa64mmfr0, &value, sizeof(uint64_t));
	pr_info("aa64mmfr0: pa_range:0x%x t_gran_16k:0x%x t_gran_64k"
		 ":0x%x t_gran_4k:0x%x\n", aa64mmfr0.pa_range,
		 aa64mmfr0.t_gran_16k, aa64mmfr0.t_gran_64k,
		 aa64mmfr0.t_gran_4k);

	value = read_sysreg(CTR_EL0);
	dcache = 4 << ((value & 0xf0000) >> 16);
	icache = 4 << ((value & 0xf));
	pr_info("dcache_line_size:%d ichache_line_size:%d\n", dcache, icache);

	return 0;
}

static void vmsa_state_init(struct vcpu *vcpu, void *context)
{
	struct vm *vm = vcpu->vm;
	struct vmsa_context *c = (struct vmsa_context *)context;

	c->vtcr_el2 = generate_vtcr_el2();
	c->vttbr_el2 = generate_vttbr_el2(vm->vmid, vm->mm.pgd_base);
	c->ttbr0_el1 = 0;
	c->ttbr1_el1 = 0;
	c->mair_el1 = 0;
	c->tcr_el1 = 0;
	c->par_el1 = 0;
	c->amair_el1 = 0;
}

static void vmsa_state_save(struct vcpu *vcpu, void *context)
{
	struct vmsa_context *c = (struct vmsa_context *)context;

	dsb();
	c->vtcr_el2 = read_sysreg(VTCR_EL2);
	c->vttbr_el2 = read_sysreg(VTTBR_EL2);
	c->ttbr0_el1 = read_sysreg(TTBR0_EL1);
	c->ttbr1_el1 = read_sysreg(TTBR1_EL1);
	c->mair_el1 = read_sysreg(MAIR_EL1);
	c->tcr_el1 = read_sysreg(TCR_EL1);
	c->par_el1 = read_sysreg(PAR_EL1);
	c->amair_el1 = read_sysreg(AMAIR_EL1);
}

static void vmsa_state_restore(struct vcpu *vcpu, void *context)
{
	struct vmsa_context *c = (struct vmsa_context *)context;

	write_sysreg(c->vtcr_el2, VTCR_EL2);
	write_sysreg(c->vttbr_el2, VTTBR_EL2);
	write_sysreg(c->ttbr0_el1, TTBR0_EL1);
	write_sysreg(c->ttbr1_el1, TTBR1_EL1);
	write_sysreg(c->mair_el1, MAIR_EL1);
	write_sysreg(c->amair_el1, AMAIR_EL1);
	write_sysreg(c->tcr_el1, TCR_EL1);
	write_sysreg(c->par_el1, PAR_EL1);
	dsb();
	flush_local_tlb_guest();
}

static int vmsa_vmodule_init(struct vmodule *vmodule)
{
	vmodule->context_size = sizeof(struct vmsa_context);
	vmodule->pdata = NULL;
	vmodule->state_init = vmsa_state_init;
	vmodule->state_save = vmsa_state_save;
	vmodule->state_restore = vmsa_state_restore;

	return 0;
}

MINOS_MODULE_DECLARE(vmsa, "armv8-mmu", (void *)vmsa_vmodule_init);
