#ifndef _MVISOR_EXCEPTION_H_
#define _MVISOR_EXCEPTION_H_

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

typedef unsigned long (*ec_handler_t)(uint32_t iss, uint32_t il, void *arg);

typedef struct _ec_config_t {
	vmm_ec_t type;
	int aarch;
	ec_handler_t handler;
	int32_t ret_addr_adjust;
} ec_config_t;

#endif
