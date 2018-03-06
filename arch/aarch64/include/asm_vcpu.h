#ifndef _MVISOR_ASM_VCPU_H_
#define _MVISOR_ASM_VCPU_H_

#include <mvisor/types.h>

typedef struct _pt_regs {
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
	uint64_t x12;
	uint64_t x13;
	uint64_t x14;
	uint64_t x15;
	uint64_t x16;
	uint64_t x17;
	uint64_t x18;
	uint64_t x19;
	uint64_t x20;
	uint64_t x21;
	uint64_t x22;
	uint64_t x23;
	uint64_t x24;
	uint64_t x25;
	uint64_t x26;
	uint64_t x27;
	uint64_t x28;
	uint64_t x29;
	uint64_t x30_lr;
	uint64_t sp_el1;
	uint64_t elr_el2;
	uint64_t spsr_el2;
	uint64_t nzcv;
} pt_regs __attribute__ ((__aligned__ (sizeof(unsigned long))));

struct gic_context {
	uint64_t ich_ap0r0_el2;
	uint64_t ich_ap1r0_el2;
	uint64_t ich_eisr_el2;
	uint64_t ich_elrsr_el2;
	uint64_t ich_hcr_el2;
	uint64_t ich_lr0_el2;
	uint64_t ich_lr1_el2;
	uint64_t ich_lr2_el2;
	uint64_t ich_lr3_el2;
	uint64_t ich_lr4_el2;
	uint64_t ich_lr5_el2;
	uint64_t ich_lr6_el2;
	uint64_t ich_lr7_el2;
	uint64_t ich_lr8_el2;
	uint64_t ich_lr9_el2;
	uint64_t ich_lr10_el2;
	uint64_t ich_lr11_el2;
	uint64_t ich_lr12_el2;
	uint64_t ich_lr13_el2;
	uint64_t ich_lr14_el2;
	uint64_t ich_lr15_el2;
	uint64_t ich_misr_el2;
	uint64_t ich_vmcr_el2;
	uint64_t ich_vtr_el2;
	uint64_t icv_ap0r0_el1;
	uint64_t icv_ap1r0_el1;
	uint64_t icv_bpr0_el1;
	uint64_t icv_bpr1_el1;
	uint64_t icv_ctlr_el1;
	uint64_t icv_dir_el1;
	uint64_t icv_eoir0_el1;
	uint64_t icv_eoir1_el1;
	uint64_t icv_hppir0_el1;
	uint64_t icv_hppir1_el1;
	uint64_t icv_iar0_el1;
	uint64_t icv_iar1_el1;
	uint64_t icv_igrpen0_el1;
	uint64_t icv_igrpen1_el1;
	uint64_t icv_pmr_el1;
	uint64_t icv_rpr_el1;
} __attribute__ ((__aligned__ (sizeof(unsigned long))));

struct system_context {
	uint64_t vbar_el1;
	uint64_t esr_el1;
	uint64_t vmpidr;
	uint64_t sctlr_el1;
	uint64_t hcr_el2;
} __attribute__ ((__aligned__ (sizeof(unsigned long))));

struct aarch64_vcpu {
	struct gic_context gic_context;
	struct system_context system_context;
};

#endif
