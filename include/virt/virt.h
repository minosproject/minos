#ifndef __MINOS_VIRT_H__
#define __MINOS_VIRT_H__

struct task;

void save_vcpu_context(struct task *task);
void restore_vcpu_context(struct task *task);

int virt_init(void);

void start_vm(int vmid);
void start_all_vm(void);

#endif
