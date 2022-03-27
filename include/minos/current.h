#ifndef __MINOS_CURRENT_H__
#define __MINOS_CURRENT_H__

#include <minos/task_info.h>
#include <minos/task_def.h>

#define current			get_current_task()
#define current_task_info	get_current_task_info()
#define current_pid		current->pid
#define current_tid		current->tid
#define current_regs		(gp_regs *)current->stack_base

#endif
