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

#include <asm/aarch64_common.h>
#include <asm/aarch64_helper.h>
#include <minos/vcpu.h>
#include <minos/vmodule.h>
#include <asm/arch.h>
#include <minos/string.h>
#include <minos/print.h>
#include <minos/sched.h>
#include <minos/calltrace.h>
#include <minos/smp.h>
#include <minos/of.h>
#include <minos/platform.h>
#include <minos/task.h>

extern int el2_stage1_init(void);
extern void fdt_vm0_init(struct vm *vm);
extern int fdt_early_init(void *setup_data);
extern int fdt_init(void);
extern int fdt_spin_table_init(phy_addr_t *smp_holding);

void arch_set_virq_flag(void)
{
	uint64_t hcr_el2;

	dsb();
	hcr_el2 = read_sysreg(HCR_EL2);
	hcr_el2 |= HCR_EL2_VI;
	write_sysreg(hcr_el2, HCR_EL2);
	dsb();
}

void arch_clear_virq_flag(void)
{
	uint64_t hcr_el2;

	dsb();
	hcr_el2 = read_sysreg(HCR_EL2);
	hcr_el2 &= ~HCR_EL2_VI;
	write_sysreg(hcr_el2, HCR_EL2);
	dsb();
}

static void dump_register(gp_regs *regs)
{
	unsigned long spsr;

	if (!regs)
		return;

	spsr = regs->spsr_elx;
	pr_fatal("SPSR:0x%x Mode:%d-%s F:%d I:%d A:%d D:%d NZCV:%x\n",
			spsr, (spsr & 0x7), (spsr & 0x8) ?
			"aarch64" : "aarch32", (spsr & (BIT(6))) >> 6,
			(spsr & (BIT(7))) >> 7, (spsr & (BIT(8))) >> 8,
			(spsr & (BIT(9))) >> 9, spsr >> 28);

	pr_fatal("x0:0x%p x1:0x%p x2:0x%p\n",
			regs->x0, regs->x1, regs->x2);
	pr_fatal("x3:0x%p x4:0x%p x5:0x%p\n",
			regs->x3, regs->x4, regs->x5);
	pr_fatal("x6:0x%p x7:0x%p x8:0x%p\n",
			regs->x6, regs->x7, regs->x8);
	pr_fatal("x9:0x%p x10:0x%p x11:0x%p\n",
			regs->x9, regs->x10, regs->x11);
	pr_fatal("x12:0x%p x13:0x%p x14:0x%p\n",
			regs->x12, regs->x13, regs->x14);
	pr_fatal("x15:0x%p x16:0x%p x17:0x%p\n",
			regs->x15, regs->x16, regs->x17);
	pr_fatal("x18:0x%p x19:0x%p x20:0x%p\n",
			regs->x18, regs->x19, regs->x20);
	pr_fatal("x21:0x%p x22:0x%p x23:0x%p\n",
			regs->x21, regs->x22, regs->x23);
	pr_fatal("x24:0x%p x25:0x%p x26:0x%p\n",
			regs->x24, regs->x25, regs->x26);
	pr_fatal("x27:0x%p x28:0x%p x29:0x%p\n",
			regs->x27, regs->x28, regs->x29);
	pr_fatal("lr:0x%p esr:0x%p spsr:0x%p\n",
			regs->lr, regs->esr_elx, regs->spsr_elx);
	pr_fatal("elr:0x%p\n", regs->elr_elx);
}

void arch_dump_stack(gp_regs *regs, unsigned long *stack)
{
	struct task *task = get_current_task();
	extern unsigned char __el2_stack_end;
	unsigned long stack_base;
	unsigned long fp, lr = 0;

	if ((task) && !(is_idle_task(task))) {
		pr_fatal("current task: pid:%d prio:%d name:%s\n",
				get_task_pid(task), get_task_prio(task), task->name);
		stack_base = (unsigned long)task->stack_origin;
	} else {
		stack_base = (unsigned long)&__el2_stack_end -
			(smp_processor_id() << CONFIG_IDLE_VCPU_STACK_SHIFT);
	}

	dump_register(regs);

	if (!stack) {
		if (regs) {
			fp = regs->x29;
			lr = regs->elr_elx;
		} else {
			fp = arch_get_fp();
			lr = arch_get_lr();
		}
	} else {
		fp = *stack;
	}

	pr_fatal("Call Trace :\n");
	pr_fatal("------------ cut here ------------\n");
	do {
		print_symbol(lr);

		if ((fp < (stack_base - VCPU_DEFAULT_STACK_SIZE))
				|| (fp >= stack_base))
				break;

		lr = *(unsigned long *)(fp + sizeof(unsigned long));
		lr -= 4;
		fp = *(unsigned long *)fp;
	} while (1);
}

int arch_taken_from_guest(gp_regs *regs)
{
	/* TBD */
	return ((regs->spsr_elx & 0xf) != (AARCH64_SPSR_EL2h));
}

void arch_init_task(struct task *task, void *entry, void *arg)
{
	gp_regs *regs;

	regs = stack_to_gp_regs(task->stack_origin);
	memset(regs, 0, sizeof(gp_regs));
	task->stack_base = task->stack_origin - sizeof(gp_regs);

	regs->x0 = (uint64_t)arg;
	regs->elr_elx = (uint64_t)entry;
	regs->spsr_elx = AARCH64_SPSR_EL2h | AARCH64_SPSR_F | \
				 AARCH64_SPSR_I | AARCH64_SPSR_A;
}

int arch_early_init(void *setup_data)
{
	pr_info("current EL is 0x%x\n", GET_EL(read_CurrentEl()));

#ifdef CONFIG_VIRT
	if (!IS_IN_EL2())
		panic("minos must run at EL2 mode\n");
#endif

#ifdef CONFIG_DEVICE_TREE
	fdt_early_init(setup_data);
#endif
	return 0;
}

int __arch_init(void)
{
#ifdef CONFIG_DEVICE_TREE
	fdt_init();
#endif
	return 0;
}

void arch_smp_init(phy_addr_t *smp_h_addr)
{
#ifdef CONFIG_DEVICE_TREE
	fdt_spin_table_init(smp_h_addr);
#endif
}
