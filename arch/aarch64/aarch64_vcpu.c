#include <asm/aarch64_common.h>
#include <asm/aarch64_helper.h>
#include <minos/task.h>
#include <asm/arch.h>

void arch_init_task(struct task *task, void *entry)
{
	gp_regs *regs;

	regs = stack_to_gp_regs(task->stack_base);
	memset((char *)regs, 0, sizeof(gp_regs));

	regs->elr_elx = (uint64_t)entry;

	if (task->task_type == TASK_TYPE_VCPU) {
		regs->spsr_elx = AARCH64_SPSR_EL1h | AARCH64_SPSR_F | \
				 AARCH64_SPSR_I | AARCH64_SPSR_A;

	} else {
		regs->spsr_elx = AARCH64_SPSR_EL2h | AARCH64_SPSR_F | \
				 AARCH64_SPSR_I | AARCH64_SPSR_A;

	}
}
