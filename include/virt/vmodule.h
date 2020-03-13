#ifndef _MINOS_MODULE_H_
#define _MINOS_MODULE_H_

#include <minos/types.h>
#include <minos/list.h>
#include <minos/device_id.h>

struct vcpu;

#define INVALID_MODULE_ID (0xffff)

struct vmodule {
	char name[32];
	int id;
	struct list_head list;
	uint32_t context_size;

	/*
	 * below member usually used for vcpu vcpu
	 *
	 * state_save - save the context when sched out
	 * state_restore - restore the context when sched in
	 * state_init - init the state when the vcpu is create
	 * state_deinit - destroy the state when the vcpu is releas
	 * state_reset - reset the state when the vcpu is reset
	 * state_stop - stop the state when the vcpu is stop
	 * state_suspend - suspend the state when the vcpu suspend
	 * state_resume - resume the state when the vcpu is resume
	 */
	void (*state_save)(struct vcpu *vcpu, void *context);
	void (*state_restore)(struct vcpu *vcpu, void *context);
	void (*state_init)(struct vcpu *vcpu, void *context);
	void (*state_deinit)(struct vcpu *vcpu, void *context);
	void (*state_reset)(struct vcpu *vcpu, void *context);
	void (*state_stop)(struct vcpu *vcpu, void *context);
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
void stop_vcpu_vmodule_state(struct vcpu *vcpu);
int vmodules_init(void);
int register_vcpu_vmodule(const char *name, vmodule_init_fn fn);

#endif
