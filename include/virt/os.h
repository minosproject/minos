#ifndef _MINOS_OS_H_
#define _MINOS_OS_H_

struct vcpu;
struct vm;

#define MINOS_OS_NAME_SIZE	(32)

enum {
	OS_TYPE_OTHERS = 0,
	OS_TYPE_LINUX,
	OS_TYPE_XNU,
	OS_TYPE_MAX,
};

struct os_ops {
	void (*vm_init)(struct vm *vm);
	void (*vcpu_init)(struct vcpu *vcpu);
	void (*vcpu_power_on)(struct vcpu *vcpu, unsigned long entry);
	void (*vm_setup)(struct vm *vm);
	int (*create_guest_vm_resource)(struct vm *vm);
	int (*create_native_vm_resource)(struct vm *vm);
};

struct os {
	char name[MINOS_OS_NAME_SIZE];
	int type;
	struct os_ops *ops;
};

int register_os(char *name, int type, struct os_ops *ops);
struct os *alloc_os(char *name, int type);
struct os *get_vm_os(char *type);
void os_setup_vm(struct vm *vm);
void os_vcpu_init(struct vcpu *vcpu);
void os_vcpu_power_on(struct vcpu *vcpu, unsigned long entry);
int os_create_native_vm_resource(struct vm *vm);
int os_create_guest_vm_resource(struct vm *vm);

#endif
