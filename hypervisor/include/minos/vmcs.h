#ifndef __VMCS_H__
#define __VMCS_H__

#include <minos/types.h>

struct vmcs {
	volatile uint32_t vcpu_id;
	volatile uint32_t trap_type;
	volatile uint32_t trap_reason;
	volatile int32_t  trap_ret;
	volatile uint64_t trap_data;
	volatile uint64_t trap_result;
	volatile uint64_t host_index;
	volatile uint64_t guest_index;
	volatile uint64_t data[0];
} __align(1024);

#define VMCS_DATA_SIZE	(1024 - 48)
#define VMCS_SIZE(nr) 	PAGE_BALIGN(nr * sizeof(struct vmcs))

enum vm_trap_type {
	VMTRAP_TYPE_MMIO = 0,
	VMTRAP_TYPE_COMMON,
	VMTRAP_TYPE_UNKNOWN,
};

enum vm_trap_reason {
	VMTRAP_REASON_READ = 0,
	VMTRAP_REASON_WRITE,
	VMTRAP_REASON_CONFIG,
	VMTRAP_REASON_REBOOT,
	VMTRAP_REASON_SHUTDOWN,
	VMTRAP_REASON_UNKNOWN,
};

int vm_create_vmcs_irq(struct vm *vm, int vcpu_id);
unsigned long vm_create_vmcs(struct vm *vm);
int setup_vmcs_data(void *data, size_t size);
int __vcpu_trap(uint32_t type, uint32_t reason,
		uint64_t data, uint64_t *ret, int nonblock);

static inline int trap_vcpu(uint32_t type, uint32_t reason,
		uint64_t data, uint64_t *ret)
{
	return __vcpu_trap(type, reason, data, ret, 0);
}

static inline int trap_vcpu_nonblock(uint32_t type,
		uint32_t reason, uint64_t data, uint64_t *ret)
{
	return __vcpu_trap(type, reason, data, ret, 1);
}

#endif
