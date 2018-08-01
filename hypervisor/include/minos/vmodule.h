#ifndef _MINOS_MODULE_H_
#define _MINOS_MODULE_H_

#include <minos/types.h>
#include <minos/list.h>
#include <minos/vcpu.h>

#include <minos/device_id.h>

#define MINOS_MODULE_NAME_IRQCHIP		"irq_chip"
#define MINOS_MODULE_NAME_MMU			"mmu"
#define MINOS_MODULE_NAME_SYSTEM		"system"

#define INVAILD_MODULE_ID		(0xffff)

struct vmodule {
	char name[32];
	char type[32];
	int id;
	struct list_head list;
	uint32_t context_size;
	void *pdata;
	void *context;
	void (*state_save)(struct vcpu *vcpu, void *context);
	void (*state_restore)(struct vcpu *vcpu, void *context);
	void (*state_init)(struct vcpu *vcpu, void *context);
	void (*state_deinit)(struct vcpu *vcpu, void *context);
	void (*vm_init)(struct vm *vm);
	void (*vm_deinit)(struct vm *vm);
};

void *get_vmodule_data(unsigned long s, unsigned long e,
		int (*check)(struct module_id *vmodule));

int vcpu_vmodules_init(struct vcpu *vcpu);
int vcpu_vmodules_deinit(struct vcpu *vcpu);
void *get_vmodule_data_by_name(struct vcpu *vcpu, char *name);
void *get_vmodule_data_by_id(struct vcpu *vcpu, int id);
void save_vcpu_vmodule_state(struct vcpu *vcpu);
void restore_vcpu_vmodule_state(struct vcpu *vcpu);
int get_vmodule_id(char *type);
void vm_vmodules_init(struct vm *vm);
void vm_vmodules_deinit(struct vm *vm);
int vmodules_init(void);

#endif
