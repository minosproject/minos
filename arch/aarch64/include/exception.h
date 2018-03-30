#ifndef _MVISOR_EXCEPTION_H_
#define _MVISOR_EXCEPTION_H_

#include <config/config.h>

typedef enum _vmm_ec_t {
	EC_UNKNOWN		= 0x00,
#define EC_UNKNOWN		(0x00)
	EC_WFI_WFE		= 0x01,
#define EC_WFI_WFE		(0x01)
	EC_MCR_MRC_CP15		= 0x03,
#define EC_MCR_MRC_CP15		(0x03)
	EC_MCRR_MRRC_CP15	= 0x04,
#define EC_MCRR_MRRC_CP15	(0x04)
	EC_MCR_MRC_CP14		= 0x05,
#define EC_MCR_MRC_CP14		(0x05)
	EC_LDC_STC_CP14		= 0x06,
#define EC_LDC_STC_CP14		(0x06)
	EC_ACCESS_SIMD_REG	= 0x07,
#define EC_ACCESS_SIMD_REG	(0x07)
	EC_MCR_MRC_CP10		= 0x08,
#define EC_MCR_MRC_CP10		(0x08)
	EC_MRRC_CP14		= 0x0C,
#define EC_MRRC_CP14		(0x0C)
	EC_ILLEGAL_EXE_STATE	= 0x0e,
#define EC_ILLEGAL_EXE_STATE	(0x0e)
	EC_SVC_AARCH32		= 0x11,
#define EC_SVC_AARCH32		(0x11)
	EC_HVC_AARCH32		= 0x12,
#define EC_HVC_AARCH32		(0x12)
	EC_SMC_AARCH32		= 0x13,
#define EC_SMC_AARCH32		(0x13)
	EC_SVC_AARCH64		= 0x15,
#define EC_SVC_AARCH64		(0x15)
	EC_HVC_AARCH64		= 0x16,
#define EC_HVC_AARCH64		(0x16)
	EC_SMC_AARCH64		= 0x17,
#define EC_SMC_AARCH64		(0x17)
	EC_ACESS_SYSTEM_REG	= 0x18,
#define EC_ACESS_SYSTEM_REG	(0x18)
	EC_INSABORT_TFL		= 0x20,
#define EC_INSABORT_TFL		(0x20)
	EC_INSABORT_TWE		= 0x21,
#define EC_INSABORT_TWE		(0x21)
	EC_MISALIGNED_PC	= 0x22,
#define EC_MISALIGNED_PC	(0x22)
	EC_DATAABORT_TFL	= 0x24,
#define EC_DATAABORT_TFL	(0x24)
	EC_DATAABORT_TWE	= 0x25,
#define EC_DATAABORT_TWE	(0x25)
	EC_STACK_MISALIGN	= 0x26,
#define EC_STACK_MISALIGN	(0x26)
	EC_FLOATING_AARCH32	= 0x28,
#define EC_FLOATING_AARCH32	(0x28)
	EC_FLOATING_AARCH64	= 0x2C,
#define EC_FLOATING_AARCH64	(0x2C)
	EC_SERROR		= 0x2F,
#define EC_SERROR		(0x2F)
	EC_BREAKPOINT_TFL	= 0x30,
#define EC_BREAKPOINT_TFL	(0x30)
	EC_BREAKPOINT_TWE	= 0x31,
#define EC_BREAKPOINT_TWE	(0x31)
	EC_SOFTWARE_STEP_TFL	= 0x32,
#define EC_SOFTWARE_STEP_TFL	(0x32)
	EC_SOFTWARE_STEP_TWE	= 0x33,
#define EC_SOFTWARE_STEP_TWE	(0x33)
	EC_WATCHPOINT_TFL	= 0x34,
#define EC_WATCHPOINT_TFL	(0x34)
	EC_WATCHPOINT_TWE	= 0x35,
#define EC_WATCHPOINT_TWE	(0x35)
	EC_BKPT_INS		= 0x38,
#define EC_BKPT_INS		(0x38)
	EC_VCTOR_CATCH		= 0x3A,
#define EC_VCTOR_CATCH		(0x3A)
	EC_BRK_INS		= 0x3C,
#define EC_BRK_INS		(0x3C)
} vmm_ec_t;

#define EC_TYPE_AARCH64		(0x1)
#define EC_TYPE_AARCH32		(0X2)
#define EC_TYPE_BOTH		(0x3)

typedef int (*ec_handler_t)(vcpu_regs *reg, uint32_t esr_value);

typedef struct _ec_config_t {
	vmm_ec_t type;
	int aarch;
	ec_handler_t handler;
	int32_t ret_addr_adjust;
} ec_config_t;

/*
 * copy from xen
 */

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
#ifdef CONFIG_ARM_AARCH32
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

#ifdef CONFIG_ARM_AARCH64
struct esr_brk {
	unsigned long comment:16;   /* Comment */
        unsigned long res0:9;
        unsigned long len:1;        /* Instruction length */
        unsigned long ec:6;         /* Exception Class */
};
#endif

#endif
