#ifndef _MINOS_OS_H_
#define _MINOS_OS_H_

struct vcpu;

#define MINOS_OS_NAME_SIZE	(32)

struct os_ops {
	void (*vcpu_init)(struct vcpu *vcpu);
	void (*vcpu_power_on)(struct vcpu *vcpu, unsigned long entry);
};

struct os {
	char name[MINOS_OS_NAME_SIZE];
	struct list_head list;
	struct os_ops *ops;
};

int register_os(struct os *os);
struct os *alloc_os(char *name);
struct os *get_vm_os(char *type);

#endif
