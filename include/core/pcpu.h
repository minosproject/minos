/*
 * created by Le MIn 2017/12/09
 */

#ifndef _MVISOR_HCPU_H_
#define _MVISOR_HCPU_H_

typedef struct vmm_pcpu {
	uint32_t pcpu_id;
	struct list_head vcpu_list;
} pcpu_t;

uint32_t pcpu_affinity(vcpu_t *vcpu, uint32_t affinity);

#define PCPU_AFFINITY_FAIL	(0xffff)

#endif
