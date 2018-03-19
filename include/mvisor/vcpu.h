/*
 * created by Le MIn 2017/12/09
 */

#ifndef _MVISOR_VCPU_H_
#define _MVISOR_VCPU_H_

#include <mvisor/types.h>
#include <config/config.h>
#include <mvisor/list.h>
#include <mvisor/vm.h>

#include <asm/asm_vcpu.h>

typedef enum _vcpu_state_t {
	VCPU_STATE_READY 	= 0x0001,
	VCPU_STATE_RUNNING 	= 0x0002,
	VCPU_STATE_SLEEP 	= 0x0004,
	VCPU_STATE_STOP  	= 0x0008,
	VCPU_STATE_ERROR 	= 0xffff,
} vcpu_state_t;

#define CONFIG_VCPU_MAX_ACTIVE_IRQS	(16)

#define VIRQ_STATE_INACTIVE		(0x0)
#define VIRQ_STATE_PENDING		(0x1)
#define VIRQ_STATE_ACTIVE		(0x2)
#define VIRQ_STATE_ACTIVE_AND_PENDING	(0x3)

struct vcpu_irq {
	uint32_t h_intno;
	uint32_t v_intno;
	int state;
	int id;
	struct list_head list;
};

struct irq_struct {
	uint32_t count;
	struct list_head pending_list;
	DECLARE_BITMAP(irq_bitmap, CONFIG_VCPU_MAX_ACTIVE_IRQS);
	struct vcpu_irq vcpu_irqs[CONFIG_VCPU_MAX_ACTIVE_IRQS];
};

typedef struct vmm_vcpu {
	vcpu_regs regs;
	int guest_vm;
	uint32_t vcpu_id;
	vcpu_state_t state;
	vm_t *vm;
	unsigned long entry_point;
	uint32_t pcpu_affinity;
	uint32_t status;
	struct list_head pcpu_list;

	/*
	 * member to record the irq list which the
	 * vcpu is handling now
	 */
	struct irq_struct irq_struct;

	void **module_context;
	void *arch_data;
} vcpu_t __attribute__ ((__aligned__ (sizeof(unsigned long))));

static vcpu_state_t inline get_vcpu_state(vcpu_t *vcpu)
{
	return vcpu->state;
}

static void inline set_vcpu_state(vcpu_t *vcpu, vcpu_state_t state)
{
	vcpu->state = state;
}

static uint32_t inline get_vcpu_id(vcpu_t *vcpu)
{
	return vcpu->vcpu_id;
}

static uint32_t inline get_vmid(vcpu_t *vcpu)
{
	return (vcpu->vm->vmid);
}

static uint32_t inline get_pcpu_id(vcpu_t *vcpu)
{
	return vcpu->pcpu_affinity;
}

vcpu_t *get_vcpu_by_id(uint32_t vmid, uint32_t vcpu_id);

vm_t *get_vm_by_id(uint32_t vmid);

int arch_vm_init(vm_t *vm);

int vmm_create_vms(void);

int vm_memory_init(vm_t *vm);

#endif
