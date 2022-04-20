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
#include <asm/arch.h>
#include <minos/string.h>
#include <minos/print.h>
#include <minos/sched.h>
#include <minos/calltrace.h>
#include <minos/smp.h>
#include <minos/of.h>
#include <minos/platform.h>
#include <minos/task.h>
#include <minos/console.h>
#include <minos/ramdisk.h>
#include <asm/tcb.h>
#include <minos/vspace.h>
#include <minos/mm.h>

#ifdef CONFIG_VIRT
#define read_esr()	read_esr_el2()
#else
#define read_esr()	read_esr_el1()
#endif

#ifdef CONFIG_VIRT
extern void vcpu_context_restore(struct task *task);
extern void vcpu_context_save(struct task *task);
#endif

#ifdef CONFIG_DEVICE_TREE
extern void of_parse_host_device_tree(void);
#endif

void arch_dump_register(gp_regs *regs)
{
	unsigned long spsr;

	if (!regs)
		return;

	spsr = regs->pstate;
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
	pr_fatal("lr:0x%p sp_el0:0x%p spsr:0x%p\n",
			regs->lr, regs->sp, regs->pstate);
	pr_fatal("pc:0x%p esr:0x%p\n", regs->pc, read_esr());
}

void arch_dump_stack(gp_regs *regs, unsigned long *stack)
{
	struct task *task = get_current_task();
	unsigned long fp, lr = 0;

	if ((task) && os_is_running()) {
		pr_fatal("current task: tid:%d prio:%d name:%s\n",
				get_task_tid(task), get_task_prio(task),
				task->name);
	}

	arch_dump_register(regs);

	if (!stack) {
		if (regs) {
			fp = regs->x29;
			lr = regs->pc;
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

		if ((fp < (unsigned long)task->stack_bottom) ||
				(fp >= (unsigned long)task->stack_top))
				break;

		lr = *(unsigned long *)(fp + sizeof(unsigned long));
		lr -= 4;
		fp = *(unsigned long *)fp;
	} while (1);
}

int arch_taken_from_guest(gp_regs *regs)
{
	return !!((regs->pstate & 0xf) != (AARCH64_SPSR_EL2h));
}

int arch_is_exit_to_user(struct task *task)
{
	gp_regs *regs = (gp_regs *)task->stack_base;

	return !!((regs->pstate & 0xf) != (AARCH64_SPSR_EL2h));
}

void arch_task_sched_out(struct task *task)
{
	struct cpu_context *c = &task->cpu_context;
	extern void fpsimd_state_save(struct task *task,
		struct fpsimd_context *c);

#ifdef CONFIG_VIRT
	if (task_is_vcpu(task))
		vcpu_context_save(task);
#endif
	fpsimd_state_save(task, &c->fpsimd_state);
}

void arch_task_sched_in(struct task *task)
{
	struct cpu_context *c = &task->cpu_context;
	extern void fpsimd_state_restore(struct task *task,
		struct fpsimd_context *c);

#ifdef CONFIG_VIRT
	if (task_is_vcpu(task))
		vcpu_context_restore(task);
#endif
	fpsimd_state_restore(task, &c->fpsimd_state);
}

static void aarch64_init_kernel_task(struct task *task, gp_regs *regs)
{
	extern void aarch64_task_exit(void);

	/*
	 * if the task is not a deadloop the task will exist
	 * by itself like below
	 *	int main(int argc, char **argv)
	 *	{
	 *		do_some_thing();
	 *		return 0;
	 *	}
	 * then the lr register should store a function to
	 * handle the task's exist
	 *
	 * kernel task will not use fpsimd now, so kernel task
	 * do not need to save/restore it
	 */
	regs->lr = (uint64_t)aarch64_task_exit;

#ifdef CONFIG_VIRT
	regs->pstate = AARCH64_SPSR_EL2h;
#else
	regs->pstate = AARCH64_SPSR_EL1h;
#endif
}

void arch_init_task(struct task *task, void *entry, void *user_sp, void *arg)
{
	gp_regs *regs = stack_to_gp_regs(task->stack_top);

	memset(regs, 0, sizeof(gp_regs));
	task->stack_base = (void *)regs;

	regs->pc = (uint64_t)entry;
	regs->sp = (uint64_t)user_sp;
	regs->x18 = (uint64_t)task;
	regs->x0 = (uint64_t)arg;

	aarch64_init_kernel_task(task, regs);
}

void arch_release_task(struct task *task)
{

}

static int __init_text aarch64_init_percpu(void)
{
	uint64_t reg;

	reg = read_CurrentEl();
	pr_notice("current EL is %d\n", GET_EL(reg));

	/*
	 * set IMO and FMO let physic irq and fiq taken to
	 * EL2, without this irq and fiq will not send to
	 * the cpu
	 */
#ifdef CONFIG_VIRT
	reg = read_sysreg64(HCR_EL2);
	reg |= HCR_EL2_IMO | HCR_EL2_FMO | HCR_EL2_AMO;
	write_sysreg64(reg, HCR_EL2);
	// write_sysreg64(0x3 << 20, CPACR_EL2);
	dsb();
	isb();
#else
	write_sysreg64(0x3 << 20, CPACR_EL1);
	dsb();
	isb();
#endif

	return 0;
}
arch_initcall_percpu(aarch64_init_percpu);

int arch_early_init(void)
{
#ifdef CONFIG_DEVICE_TREE
	/*
	 * set up the platform from the dtb file then get the spin
	 * table information if the platform is using spin table to
	 * wake up other cores
	 */
	of_setup_platform();
#endif
	return 0;
}

int __arch_init(void)
{
#ifdef CONFIG_DEVICE_TREE
	of_parse_host_device_tree();
#endif
	return 0;
}

uint64_t cpuid_to_affinity(int cpuid)
{
	int aff0, aff1;

	if (cpu_has_feature(ARM_FEATURE_MPIDR_SHIFT))  {
		if (cpuid < CONFIG_NR_CPUS_CLUSTER0)
			return (cpuid << MPIDR_EL1_AFF1_LSB);
		else {
			aff0 = cpuid - CONFIG_NR_CPUS_CLUSTER0;
			aff1 = 1;

			return (aff1 << MPIDR_EL1_AFF2_LSB) |
				(aff0 << MPIDR_EL1_AFF1_LSB);
		}
	} else {
		if (cpuid < CONFIG_NR_CPUS_CLUSTER0) {
			return cpuid;
		} else {
			aff0 = cpuid - CONFIG_NR_CPUS_CLUSTER0;
			aff1 = 1;

			return (aff1 << MPIDR_EL1_AFF1_LSB) + aff0;
		}
	}
}

int affinity_to_cpuid(unsigned long affinity)
{
	int aff0, aff1;

	if (cpu_has_feature(ARM_FEATURE_MPIDR_SHIFT))  {
		aff0 = (affinity >> MPIDR_EL1_AFF1_LSB) & 0xff;
		aff1 = (affinity >> MPIDR_EL1_AFF2_LSB) & 0xff;
	} else {
		aff0 = (affinity >> MPIDR_EL1_AFF0_LSB) & 0xff;
		aff1 = (affinity >> MPIDR_EL1_AFF1_LSB) & 0xff;
	}

	return (aff1 * CONFIG_NR_CPUS_CLUSTER0) + aff0;
}

void arch_smp_init(phy_addr_t *smp_h_addr)
{
#ifdef CONFIG_DEVICE_TREE
	of_spin_table_init(smp_h_addr);
#endif
}

static void *relocate_dtb_address(unsigned long dtb)
{
	unsigned long mem_end, relocated_base = dtb;
	extern unsigned long minos_end;
	size_t size;

	ASSERT(!fdt_check_header((void *)dtb));

	/*
	 * DTB image start address should bigger than minos_end, or
	 * DTB image should included in minos.bin.
	 */
	mem_end = BALIGN(ptov(minos_end), 16);
	size = fdt_totalsize(dtb);
	if (dtb < minos_end && (dtb + size) > minos_end)
		panic("minos data overlaped by dtb image\n");

	if (dtb > mem_end) {
		pr_notice("relocate dtb from 0x%x to 0x%x\n", dtb, mem_end);
		memmove((void *)mem_end, (void *)dtb, size);

		/*
		 * call of init again to check and setup the new
		 * device tree memory address.
		 */
		relocated_base = mem_end;
		minos_end += size;
	}

	return (void *)relocated_base;
}

void arch_main(void *dtb)
{
	extern void boot_main(void);
	char *name = NULL;

	pr_notice("Starting Minos AARCH64\n");
#ifdef CONFIG_DTB_LOAD_ADDRESS
	dtb = (void *)ptov(CONFIG_DTB_LOAD_ADDRESS);
#else
	dtb = (void *)ptov(dtb);
#endif
	pr_notice("DTB address [0x%x]\n", dtb);

	dtb = relocate_dtb_address((unsigned long)dtb);

	of_init(dtb);
	of_get_console_name(&name);
	console_init(name);

	/*
	 * here start the kernel
	 */
	boot_main();
}
