#ifndef __VMCS_H__
#define __VMCS_H__

#include <sys/types.h>
#include <inttypes.h>

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

enum vm_trap_type {
	VMTRAP_TYPE_MMIO = 0,
	VMTRAP_TYPE_COMMON,
	VMTRAP_TYPE_UNKNOWN,
};

enum vm_trap_reason {
	VMTRAP_REASON_READ = 0,
	VMTRAP_REASON_WRITE,
	VMTRAP_REASON_CONFIG,
	VMTRAP_REASON_VM_RESET,
	VMTRAP_REASON_VM_SHUTDOWN,
	VMTRAP_REASON_UNKNOWN,
};

#endif
