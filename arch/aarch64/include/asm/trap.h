#ifndef _MINOS_EXCEPTION_H_
#define _MINOS_EXCEPTION_H_

#include <config/config.h>
#include <asm/arch.h>

typedef int (*sync_handler_t)(gp_regs *reg, int ec, uint32_t esr);

struct sync_desc {
	uint8_t aarch;
	uint8_t irq_safe;
	uint8_t ret_addr_adjust;
	uint8_t resv;
	sync_handler_t handler;
};

#define DEFINE_SYNC_DESC(t, arch, h, is, raa)			\
	static struct sync_desc sync_desc_##t __used = {	\
		.aarch = arch, 					\
		.handler = h,					\
		.irq_safe = is,					\
		.ret_addr_adjust = raa, 			\
	}

struct esr {
	unsigned long iss:25;  /* Instruction Specific Syndrome */
	unsigned long len:1;   /* Instruction length */
	unsigned long ec:6;    /* Exception Class */
};

/* Common to all conditional exception classes (0x0N, except 0x00). */
struct esr_cond {
	unsigned long iss:20;  /* Instruction Specific Syndrome */
	unsigned long cc:4;    /* Condition Code */
	unsigned long ccvalid:1;/* CC Valid */
	unsigned long len:1;   /* Instruction length */
	unsigned long ec:6;    /* Exception Class */
};

struct esr_wfi_wfe {
	unsigned long ti:1;    /* Trapped instruction */
	unsigned long sbzp:19;
	unsigned long cc:4;    /* Condition Code */
	unsigned long ccvalid:1;/* CC Valid */
	unsigned long len:1;   /* Instruction length */
	unsigned long ec:6;    /* Exception Class */
};

/* reg, reg0, reg1 are 4 bits on AArch32, the fifth bit is sbzp. */
struct esr_cp32 {
	unsigned long read:1;  /* Direction */
	unsigned long crm:4;   /* CRm */
	unsigned long reg:5;   /* Rt */
	unsigned long crn:4;   /* CRn */
	unsigned long op1:3;   /* Op1 */
	unsigned long op2:3;   /* Op2 */
	unsigned long cc:4;    /* Condition Code */
	unsigned long ccvalid:1;/* CC Valid */
	unsigned long len:1;   /* Instruction length */
	unsigned long ec:6;    /* Exception Class */
};

struct esr_cp64 {
	unsigned long read:1;   /* Direction */
	unsigned long crm:4;    /* CRm */
	unsigned long reg1:5;   /* Rt1 */
	unsigned long reg2:5;   /* Rt2 */
	unsigned long sbzp2:1;
	unsigned long op1:4;    /* Op1 */
	unsigned long cc:4;     /* Condition Code */
	unsigned long ccvalid:1;/* CC Valid */
	unsigned long len:1;    /* Instruction length */
	unsigned long ec:6;     /* Exception Class */
};

struct esr_cp {
	unsigned long coproc:4; /* Number of coproc accessed */
	unsigned long sbz0p:1;
	unsigned long tas:1;    /* Trapped Advanced SIMD */
	unsigned long res0:14;
	unsigned long cc:4;     /* Condition Code */
	unsigned long ccvalid:1;/* CC Valid */
	unsigned long len:1;    /* Instruction length */
	unsigned long ec:6;     /* Exception Class */
};

/*
 * This encoding is valid only for ARMv8 (ARM DDI 0487B.a, pages D7-2271 and
 * G6-4957). On ARMv7, encoding ISS for EC=0x13 is defined as UNK/SBZP
 * (ARM DDI 0406C.c page B3-1431). UNK/SBZP means that hardware implements
 * this field as Read-As-Zero. ARMv8 is backwards compatible with ARMv7:
 * reading CCKNOWNPASS on ARMv7 will return 0, which means that condition
 * check was passed or instruction was unconditional.
 */
struct esr_smc32 {
	unsigned long res0:19;  /* Reserved */
	unsigned long ccknownpass:1; /* Instruction passed conditional check */
	unsigned long cc:4;    /* Condition Code */
	unsigned long ccvalid:1;/* CC Valid */
	unsigned long len:1;   /* Instruction length */
	unsigned long ec:6;    /* Exception Class */
};

struct esr_sysreg {
	unsigned long read:1;   /* Direction */
	unsigned long crm:4;    /* CRm */
	unsigned long reg:5;    /* Rt */
	unsigned long crn:4;    /* CRn */
	unsigned long op1:3;    /* Op1 */
	unsigned long op2:3;    /* Op2 */
	unsigned long op0:2;    /* Op0 */
	unsigned long res0:3;
	unsigned long len:1;    /* Instruction length */
	unsigned long ec:6;
};

struct esr_iabt {
	unsigned long ifsc:6;  /* Instruction fault status code */
	unsigned long res0:1;  /* RES0 */
	unsigned long s1ptw:1; /* Stage 2 fault during stage 1 translation */
	unsigned long res1:1;  /* RES0 */
	unsigned long eat:1;   /* External abort type */
	unsigned long fnv:1;   /* FAR not Valid */
	unsigned long res2:14;
	unsigned long len:1;   /* Instruction length */
	unsigned long ec:6;    /* Exception Class */
};

struct esr_dabt {
	unsigned long dfsc:6;  /* Data Fault Status Code */
	unsigned long write:1; /* Write / not Read */
	unsigned long s1ptw:1; /* Stage 2 fault during stage 1 translation */
	unsigned long cache:1; /* Cache Maintenance */
	unsigned long eat:1;   /* External Abort Type */
	unsigned long fnv:1;   /* FAR not Valid */
#ifdef ARM_AARCH32
	unsigned long sbzp0:5;
#else
	unsigned long sbzp0:3;
	unsigned long ar:1;    /* Acquire Release */
	unsigned long sf:1;    /* Sixty Four bit register */
#endif
	unsigned long reg:5;   /* Register */
	unsigned long sign:1;  /* Sign extend */
	unsigned long size:2;  /* Access Size */
	unsigned long valid:1; /* Syndrome Valid */
	unsigned long len:1;   /* Instruction length */
	unsigned long ec:6;    /* Exception Class */
};

/* Contain the common bits between DABT and IABT */
struct esr_xabt {
	unsigned long fsc:6;    /* Fault status code */
	unsigned long pad1:1;   /* Not common */
	unsigned long s1ptw:1;  /* Stage 2 fault during stage 1 translation */
	unsigned long pad2:1;   /* Not common */
	unsigned long eat:1;    /* External abort type */
	unsigned long fnv:1;    /* FAR not Valid */
	unsigned long pad3:14;  /* Not common */
	unsigned long len:1;    /* Instruction length */
	unsigned long ec:6;     /* Exception Class */
};

#ifdef ARM_AARCH64
struct esr_brk {
	unsigned long comment:16;   /* Comment */
        unsigned long res0:9;
        unsigned long len:1;        /* Instruction length */
        unsigned long ec:6;         /* Exception Class */
};
#endif

#endif
