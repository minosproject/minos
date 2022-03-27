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

#include <asm/aarch64_helper.h>
#include <asm/trap.h>
#include <minos/minos.h>
#include <minos/smp.h>
#include <asm/reg.h>
#include <minos/sched.h>
#include <minos/irq.h>
#include <asm/svccc.h>
#include <asm/trap.h>

static char *mode_info[] = {
	"Sync taken from current EL with SP0",
	"IRQ taken from current EL with SP0",
	"FIQ taken from current EL with SP0",
	"Serr taken from current EL with SP0",
	"Sync taken from current EL with SPx",
	"IRQ taken from current EL with SPx",
	"FIQ taken from current EL with SPx",
	"Serr taken from current EL with SPx",
	"Sync taken from lower EL with AARCH32"
	"IRQ taken from lower EL with AARCH32"
	"FIQ taken from lower EL with AARCH32"
	"Serr taken from lower EL with AARCH32"
	"Sync taken from lower EL with AARCH64"
	"IRQ taken from lower EL with AARCH64"
	"FIQ taken from lower EL with AARCH64"
	"Serr taken from lower EL with AARCH64"
};

static const char *esr_class_str[] = {
	[0 ... ESR_ELx_EC_MAX]		= "UNRECOGNIZED EC",
	[ESR_ELx_EC_UNKNOWN]		= "Unknown/Uncategorized",
	[ESR_ELx_EC_WFx]		= "WFI/WFE",
	[ESR_ELx_EC_CP15_32]		= "CP15 MCR/MRC",
	[ESR_ELx_EC_CP15_64]		= "CP15 MCRR/MRRC",
	[ESR_ELx_EC_CP14_MR]		= "CP14 MCR/MRC",
	[ESR_ELx_EC_CP14_LS]		= "CP14 LDC/STC",
	[ESR_ELx_EC_FP_ASIMD]		= "ASIMD",
	[ESR_ELx_EC_CP10_ID]		= "CP10 MRC/VMRS",
	[ESR_ELx_EC_CP14_64]		= "CP14 MCRR/MRRC",
	[ESR_ELx_EC_ILL]		= "PSTATE.IL",
	[ESR_ELx_EC_SVC32]		= "SVC (AArch32)",
	[ESR_ELx_EC_HVC32]		= "HVC (AArch32)",
	[ESR_ELx_EC_SMC32]		= "SMC (AArch32)",
	[ESR_ELx_EC_SVC64]		= "SVC (AArch64)",
	[ESR_ELx_EC_HVC64]		= "HVC (AArch64)",
	[ESR_ELx_EC_SMC64]		= "SMC (AArch64)",
	[ESR_ELx_EC_SYS64]		= "MSR/MRS (AArch64)",
	[ESR_ELx_EC_SVE]		= "SVE",
	[ESR_ELx_EC_IMP_DEF]		= "EL3 IMP DEF",
	[ESR_ELx_EC_IABT_LOW]		= "IABT (lower EL)",
	[ESR_ELx_EC_IABT_CUR]		= "IABT (current EL)",
	[ESR_ELx_EC_PC_ALIGN]		= "PC Alignment",
	[ESR_ELx_EC_DABT_LOW]		= "DABT (lower EL)",
	[ESR_ELx_EC_DABT_CUR]		= "DABT (current EL)",
	[ESR_ELx_EC_SP_ALIGN]		= "SP Alignment",
	[ESR_ELx_EC_FP_EXC32]		= "FP (AArch32)",
	[ESR_ELx_EC_FP_EXC64]		= "FP (AArch64)",
	[ESR_ELx_EC_SERROR]		= "SError",
	[ESR_ELx_EC_BREAKPT_LOW]	= "Breakpoint (lower EL)",
	[ESR_ELx_EC_BREAKPT_CUR]	= "Breakpoint (current EL)",
	[ESR_ELx_EC_SOFTSTP_LOW]	= "Software Step (lower EL)",
	[ESR_ELx_EC_SOFTSTP_CUR]	= "Software Step (current EL)",
	[ESR_ELx_EC_WATCHPT_LOW]	= "Watchpoint (lower EL)",
	[ESR_ELx_EC_WATCHPT_CUR]	= "Watchpoint (current EL)",
	[ESR_ELx_EC_BKPT32]		= "BKPT (AArch32)",
	[ESR_ELx_EC_VECTOR32]		= "Vector catch (AArch32)",
	[ESR_ELx_EC_BRK64]		= "BRK (AArch64)",
};

void bad_mode(gp_regs *regs, int mode)
{
	pr_fatal("bad error: %s\n", mode_info[mode]);
	arch_dump_register(regs);
	panic("Bad error received\n");
}

static const char *get_ec_class_string(int ec)
{
	return esr_class_str[ec];
}

static int kernel_mem_fault(gp_regs *regs, int ec, uint32_t esr)
{
	__panic(regs, "Memory fault in kernel space\n");
}

static int unknown_trap_handler(gp_regs *regs, int ec, uint32_t esr)
{
	__panic(regs, "Unknown exception class: ESR: 0x%x -- %s\n",
			esr, get_ec_class_string(ec));

	return 0;
}

DEFINE_SYNC_DESC(trap_unknown, EC_TYPE_AARCH64, unknown_trap_handler, 1, 0);
DEFINE_SYNC_DESC(trap_kernel_da, EC_TYPE_AARCH64, kernel_mem_fault, 1, 0);
DEFINE_SYNC_DESC(trap_kernel_ia, EC_TYPE_AARCH64, kernel_mem_fault, 1, 0);

static struct sync_desc *process_sync_descs[] = {
	[0 ... ESR_ELx_EC_MAX] 	= &sync_desc_trap_unknown,
	[ESR_ELx_EC_IABT_CUR]	= &sync_desc_trap_kernel_ia,
	[ESR_ELx_EC_DABT_CUR]	= &sync_desc_trap_kernel_da,
};

static inline uint32_t read_esr(void)
{
#ifdef CONFIG_VIRT
	return read_esr_el2();
#else
	return read_esr_el1();
#endif
}

static void handle_sync_exception(gp_regs *regs)
{
	uint32_t esr_value;
	uint32_t ec_type;
	struct sync_desc *ec;

	esr_value = read_esr();
	ec_type = ESR_ELx_EC(esr_value);
	if (ec_type >= ESR_ELx_EC_MAX)
		panic("unknown sync exception type from current EL %d\n", ec_type);

	/*
	 * for normal userspace process the return address shall
	 * be adjust
	 */
	ec = process_sync_descs[ec_type];
	regs->pc += ec->ret_addr_adjust;
	ec->handler(regs, ec_type, esr_value);
}

void sync_exception_from_current_el(gp_regs *regs)
{
	handle_sync_exception(regs);
}

void sync_exception_from_lower_el(gp_regs *regs)
{
#ifdef CONFIG_VIRT
	extern void handle_vcpu_sync_exception(gp_regs *regs);

	/*
	 * check whether this task is a vcpu task or a normal
	 * userspace task.
	 * 1 - TGE bit means a normal task
	 * 2 - current->flags
	 */
	if ((current->flags & TASK_FLAGS_VCPU) && !(read_hcr_el2() & HCR_EL2_TGE))
		handle_vcpu_sync_exception(regs);
	else
#endif
		handle_sync_exception(regs);
}
