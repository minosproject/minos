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
	uint32_t vcpu_id;
	vm_t *vm;
	unsigned long entry_point;
	uint32_t pcpu_affinity;
	struct list_head pcpu_list;

	/*
	 * member to record the irq list which the
	 * vcpu is handling now
	 */
	struct irq_struct irq_struct;

	/*
	 * below members is used to sched
	 */
	int state;
	struct list_head state_list;

	void **module_context;
	void *arch_data;
} vcpu_t __attribute__ ((__aligned__ (sizeof(unsigned long))));

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

vcpu_t *get_vcpu_in_vm(vm_t *vm, uint32_t vcpu_id);

#endif
