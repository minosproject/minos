#ifndef _MVISOR_MODULE_H_
#define _MVISOR_MODULE_H_

#include <mvisor/types.h>
#include <mvisor/list.h>
#include <mvisor/vcpu.h>

#include <mvisor/device_id.h>

#define VMM_MODULE_NAME_IRQCHIP		"irq_chip"
#define VMM_MODULE_NAME_MMU		"mmu"
#define VMM_MODULE_NAME_SYSTEM		"system"

#define INVAILD_MODULE_ID		(0xffff)

struct vmm_module {
	char name[32];
	char type[32];
	int id;
	struct list_head list;
	uint32_t context_size;
	void *pdata;
	void *context;
	void (*exit_from_guest)(vcpu_t *vcpu, void *context);
	void (*return_to_guest)(vcpu_t *vcpu, void *context);
	void (*state_save)(vcpu_t *vcpu, void *context);
	void (*state_restore)(vcpu_t *vcpu, void *context);
	void (*state_init)(vcpu_t *vcpu, void *context);
};

int vcpu_modules_init(vcpu_t *vcpu);

void *vmm_get_module_pdata(char *name, char *type);

void *get_vcpu_module_data(vcpu_t *vcpu, char *name);

#endif
