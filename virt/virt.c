#include <minos/minos.h>
#include <virt/vmodule.h>
#include <minos/sched.h>
#include <minos/arch.h>
#include <virt/virt.h>

extern void virqs_init(void);
extern void parse_vm_config(void);

extern struct virt_config virt_config;
struct virt_config *mv_config = &virt_config;

int taken_from_guest(gp_regs *regs)
{
	return arch_taken_from_guest(regs);
}

void exit_from_guest(struct task *task, gp_regs *regs)
{
	do_hooks(task, (void *)regs, MINOS_HOOK_TYPE_EXIT_FROM_GUEST);
}

void enter_to_guest(struct task *task, gp_regs *regs)
{
	do_hooks(task, (void *)regs, MINOS_HOOK_TYPE_ENTER_TO_GUEST);
}

void save_vcpu_task_state(struct task *task)
{
	save_vcpu_vmodule_state(task_to_vcpu(task));
}

void restore_vcpu_task_state(struct task *task)
{
	restore_vcpu_vmodule_state(task_to_vcpu(task));
	enter_to_guest(task, NULL);
}

int virt_init(void)
{
	vmodules_init();

	if (create_vms() == 0)
		return -ENOENT;

	parse_vm_config();
	vms_init();
	virqs_init();

	return 0;
}
