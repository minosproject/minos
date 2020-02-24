/*
 * created by Le MIn 2017/12/09
 */

#ifndef _MVISOR_VCPU_H_
#define _MVISOR_VCPU_H_

#include <core/types.h>
#include <config/mvisor_config.h>

struct vmm_vcpu_context {
	uint64_t x0;
	uint64_t x1;
	uint64_t x2;
	uint64_t x3;
	uint64_t x4;
	uint64_t x5;
	uint64_t x6;
	uint64_t x7;
	uint64_t x8;
	uint64_t x9;
	uint64_t x10;
	uint64_t x11;
};

struct vmm_vcpu {
	uint32_t vcpu_id;
	uint32_t vm_belong_to;
	uint32_t hcpu_affinity;
	struct vmm_vcpu_context context;
};

#endif
