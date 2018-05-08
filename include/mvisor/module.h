#ifndef _MVISOR_MODULE_H_
#define _MVISOR_MODULE_H_

#include <mvisor/types.h>
#include <mvisor/list.h>
#include <mvisor/vcpu.h>

#include <mvisor/device_id.h>

#define MVISOR_MODULE_NAME_IRQCHIP		"irq_chip"
#define MVISOR_MODULE_NAME_MMU			"mmu"
#define MVISOR_MODULE_NAME_SYSTEM		"system"

#define INVAILD_MODULE_ID		(0xffff)

struct mvisor_module {
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
	void (*create_vm)(struct vm *vm);
};

int vcpu_modules_init(struct vcpu *vcpu);
void *mvisor_get_module_pdata(char *name, char *type);
void *get_module_data_by_name(struct vcpu *vcpu, char *name);
void *get_module_data_by_id(struct vcpu *vcpu, int id);
void save_vcpu_module_state(struct vcpu *vcpu);
void restore_vcpu_module_state(struct vcpu *vcpu);
int get_module_id(char *type);
void modules_create_vm(struct vm *vm);

#endif
