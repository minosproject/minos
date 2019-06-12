#ifndef _MINOS_MODULE_H_
#define _MINOS_MODULE_H_

#include <minos/types.h>
#include <minos/list.h>
#include <minos/vcpu.h>

#include <minos/device_id.h>

#define INVALID_MODULE_ID (0xffff)

struct vmodule {
	char name[32];
	int id;
	struct list_head list;
	uint32_t context_size;
	void *pdata;
	void *context;
	void (*state_save)(struct vcpu *vcpu, void *context);
	void (*state_restore)(struct vcpu *vcpu, void *context);
	void (*state_init)(struct vcpu *vcpu, void *context);
	void (*state_deinit)(struct vcpu *vcpu, void *context);
	void (*state_reset)(struct vcpu *vcpu, void *context);
	void (*state_suspend)(struct vcpu *vcpu, void *context);
	void (*state_resume)(struct vcpu *vcpu, void *context);
};

typedef int (*vmodule_init_fn)(struct vmodule *);

int vcpu_vmodules_init(struct vcpu *vcpu);
int vcpu_vmodules_deinit(struct vcpu *vcpu);
int vcpu_vmodules_reset(struct vcpu *vcpu);
void *get_vmodule_data_by_id(struct vcpu *vcpu, int id);
void *get_vmodule_data_by_name(struct vcpu *vcpu, const char *name);
void save_vcpu_vmodule_state(struct vcpu *vcpu);
void restore_vcpu_vmodule_state(struct vcpu *vcpu);
void suspend_vcpu_vmodule_state(struct vcpu *vcpu);
void resume_vcpu_vmodule_state(struct vcpu *vcpu);
int vmodules_init(void);
int register_vcpu_vmodule(const char *name, vmodule_init_fn fn);

#endif
