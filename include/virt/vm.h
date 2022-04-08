/*
 * created by Le MIn 2017/12/09
 */

#ifndef _MINOS_VM_H_
#define _MINOS_VM_H_

#include <minos/types.h>
#include <minos/list.h>
#include <config/config.h>
#include <virt/vmm.h>
#include <minos/errno.h>
#include <uapi/hypervisor.h>
#include <minos/task.h>
#include <minos/sched.h>
#include <minos/event.h>

#define VM_MAX_VCPU CONFIG_NR_CPUS

#define VM_STATE_OFFLINE (0)
#define VM_STATE_ONLINE (1)
#define VM_STATE_SUSPEND (2)
#define VM_STATE_REBOOT (3)

#define VCPU_MAX_LOCAL_IRQS		(32)
#define CONFIG_VCPU_MAX_ACTIVE_IRQS	(16)

#define VCPU_NAME_SIZE		(64)

#define VCPU_KICK_REASON_NONE 0x0
#define VCPU_KICK_REASON_HIRQ 0x1
#define VCPU_KICK_REASON_VIRQ 0x2

struct os;
struct vm;
struct virq_struct;
struct virq_chip;
struct device_node;

extern struct list_head vm_list;
extern struct list_head mem_list;

#define VCPU_STATE_RUNNING TASK_STATE_RUNNING
#define VCPU_STATE_IDLE TASK_STATE_WAIT_EVENT
#define VCPU_STATE_STOP TASK_STATE_STOP
#define VCPU_STATE_SUSPEND TASK_STATE_SUSPEND

enum {
	IN_GUEST_MODE,
	OUTSIDE_GUEST_MODE,
	IN_ROOT_MODE,
	OUTSIDE_ROOT_MODE,
};

struct vcpu {
	uint32_t vcpu_id;
	struct vm *vm;
	struct task *task;
	struct vcpu *next;

	volatile int mode;

	/*
	 * member to record the irq list which the
	 * vcpu is handling now
	 */
	struct virq_struct *virq_struct;

	struct event vcpu_event;

	struct vmcs *vmcs;
	int vmcs_irq;

	/*
	 * context for this vcpu.
	 */
	void **context;
} __cache_line_align;

struct vm {
	int vmid;
	uint32_t vcpu_nr;
	int state;
	unsigned long flags;
	uint32_t vcpu_affinity[VM_MAX_VCPU];
	void *entry_point;
	void *setup_data;
	void *load_address;
	int native;

	struct ramdisk_file *kernel_file;
	struct ramdisk_file *dtb_file;
	struct ramdisk_file *initrd_file;

	char name[VM_NAME_SIZE];
	struct vcpu **vcpus;
	struct list_head vcpu_list;
	struct mm_struct mm;
	struct os *os;
	struct list_head vm_list;
	struct device_node *dev_node;	/* the device node in dts */

	unsigned long time_offset;

	struct list_head vdev_list;

	uint32_t vspi_nr;
	int virq_same_page;
	struct virq_desc *vspi_desc;
	unsigned long *vspi_map;
	struct virq_chip *virq_chip;
	uint32_t vtimer_virq;

	void *vmcs;
	void *hvm_vmcs;
	void *resource;

	void *os_data;

	void *arch_data;

	struct vm_iommu iommu;
} __align(sizeof(unsigned long));

#define vm_name(vm)	devnode_name(vm->dev_node)

extern struct vm *vms[CONFIG_MAX_VM];
extern int total_vms;

#define for_each_vm(vm)	\
	list_for_each_entry(vm, &vm_list, vm_list)

#define vm_for_each_vcpu(vm, vcpu)	\
	for (vcpu = vm->vcpus[0]; vcpu != NULL; vcpu = vcpu->next)

#define current_vcpu (struct vcpu *)current->pdata

static int inline get_vcpu_id(struct vcpu *vcpu)
{
	return vcpu->vcpu_id;
}

static int inline get_vmid(struct vcpu *vcpu)
{
	return (vcpu->vm->vmid);
}

static int inline vcpu_affinity(struct vcpu *vcpu)
{
	return vcpu->task->affinity;
}

static inline struct vm *vcpu_to_vm(struct vcpu *vcpu)
{
	return vcpu->vm;
}

static inline struct vcpu *task_to_vcpu(struct task *task)
{
	return (struct vcpu *)task->pdata;
}

static inline struct vcpu *get_current_vcpu(void)
{
	return task_to_vcpu(get_current_task());
}

static inline struct vm *task_to_vm(struct task *task)
{
	struct vcpu *vcpu = (struct vcpu *)task->pdata;

	return vcpu->vm;
}

static inline struct vm* get_current_vm(void)
{
	return task_to_vm(get_current_task());
}

struct vcpu *get_vcpu_in_vm(struct vm *vm, uint32_t vcpu_id);
struct vcpu *get_vcpu_by_id(uint32_t vmid, uint32_t vcpu_id);

struct vcpu *create_idle_vcpu(void);
int vm_vcpus_init(struct vm *vm);

int vcpu_idle(struct vcpu *vcpu);
int vcpu_suspend(struct vcpu *vcpu, gp_regs *c,
		uint32_t state, unsigned long entry);
int vcpu_off(struct vcpu *vcpu);
int vcpu_power_on(struct vcpu *caller, unsigned long affinity,
		unsigned long entry, unsigned long unsed);
int vcpu_power_off(struct vcpu *vcpu, int timeout);
int kick_vcpu(struct vcpu *vcpu, int preempt);

struct vm *create_vm(struct vmtag *vme, struct device_node *node);
int create_guest_vm(struct vmtag *tag);
void destroy_vm(struct vm *vm);

struct vm *get_host_vm(void);

static inline struct vm *get_vm_by_id(uint32_t vmid)
{
	if (unlikely(vmid >= CONFIG_MAX_VM) || unlikely(vmid == 0))
		return NULL;
	return vms[vmid];
}

static inline int vm_is_host_vm(struct vm *vm)
{
	return !!(vm->flags & VM_FLAGS_HOST);
}

static inline int vm_is_32bit(struct vm *vm)
{
	return vm->flags & VM_FLAGS_32BIT;
}

static inline int vm_is_native(struct vm *vm)
{
	return !!(vm->flags & VM_FLAGS_NATIVE);
}

static inline int vm_id(struct vm *vm)
{
	return vm->vmid;
}

int create_vm_mmap(int vmid,  unsigned long offset,
		unsigned long size, unsigned long *addr);
int vm_create_host_vdev(struct vm *vm);
int request_vm_virqs(struct vm *vm, int base, int nr);

void arch_init_vcpu(struct vcpu *vcpu, void *entry, void *arg);

static inline int check_vcpu_state(struct vcpu *vcpu, int state)
{
	return (vcpu->task->state == state);
}

static inline int check_vm_state(struct vm *vm, int state)
{
	return (vm->state == state);
}

int start_native_vm(struct vm *vm);

int start_guest_vm(struct vm *vm);

#endif
