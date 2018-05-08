#ifndef _MVISOR_ASM_SVCCC_H_
#define _MVISOR_ASM_SVCCC_H_

#include <asm/asm_vcpu.h>

#define SVC_CTYPE_MASK			(0x80000000)
#define SVC_BTYPE_MASK			(0x40000000)
#define SVC_STYPE_MASK			(0x3f000000)
#define SVC_FID_MASK			(0x0000ffff)

#define SVC_STYPE_ARCH			(0x00)
#define SVC_STYPE_CPU			(0x01)
#define SVC_STYPE_SIP			(0x02)
#define SVC_STYPE_OEM			(0x03)
#define SVC_STYPE_STDSMC		(0x04)
#define SVC_STYPE_STDHVC		(0x05)
#define SVC_STYPE_VNDHVC		(0x06)
#define SVC_STYPE_TRUST_APP_START	(0x30)
#define SVC_STYPE_TRUST_APP_END		(0x31)
#define SVC_STYPE_TRUST_OS_START	(0x32)
#define SVC_STYPE_TRUST_OS_END		(0x3f)
#define SVC_STYPE_MAX			(64)

#define PSCI_RET_SUCCESS			0
#define PSCI_RET_NOT_SUPPORTED			-1
#define PSCI_RET_INVALID_PARAMS			-2
#define PSCI_RET_DENIED				-3
#define PSCI_RET_ALREADY_ON			-4
#define PSCI_RET_ON_PENDING			-5
#define PSCI_RET_INTERNAL_FAILURE		-6
#define PSCI_RET_NOT_PRESENT			-7
#define PSCI_RET_DISABLED			-8
#define PSCI_RET_INVALID_ADDRESS		-9

#define SVC_RET0(reg, r)	{	\
	return r;			\
}

#define SVC_RET1(reg, r, a0)	{	\
	set_reg_value(reg, 0, a0); 	\
	return r;			\
}

#define SVC_RET2(reg, r, a0, a1)	{	\
	set_reg_value(reg, 0, a0);	\
	set_reg_value(reg, 1, a1);	\
	return r;			\
}

#define SVC_RET3(reg, r, a0, a1, a2)	{	\
	set_reg_value(reg, 0, a0); 	\
	set_reg_value(reg, 1, a1); 	\
	set_reg_value(reg, 2, a2); 	\
	return r;			\
}

#define SVC_RET4(reg, r, a0, a1, a2, a3) {	\
	set_reg_value(reg, 0, a0); 	\
	set_reg_value(reg, 1, a1); 	\
	set_reg_value(reg, 2, a2); 	\
	set_reg_value(reg, 3, a3); 	\
	return r;			\
}

typedef int (*svc_handler_t)(vcpu_regs *c, uint32_t id, uint64_t *args);

struct svc_desc {
	char *name;
	uint16_t type_start;
	uint16_t type_end;
	svc_handler_t handler;
};

#define DEFINE_SMC_HANDLER(n, start, end, h)	\
	static struct svc_desc __smc_##handler __used \
	__section(".__mvisor_smc_handler") = {	\
		.name = n,	\
		.type_start = start, \
		.type_end = end, \
		.handler = h, \
	}

#define DEFINE_HVC_HANDLER(n, start, end, h)	\
	static struct svc_desc __hvc_##handler __used \
	__section(".__mvisor_hvc_handler") = {	\
		.name = n,	\
		.type_start = start, \
		.type_end = end, \
		.handler = h, \
	}

int do_svc_handler(vcpu_regs *regs, uint32_t svc_id,
		uint64_t *args, int smc);

static int inline do_smc_handler(vcpu_regs *regs,
		uint32_t svc_id, uint64_t *args)
{
	return do_svc_handler(regs, svc_id, args, 1);
}

static int inline do_hvc_handler(vcpu_regs *regs,
		uint32_t svc_id, uint64_t *args)
{
	return do_svc_handler(regs, svc_id, args, 0);
}

#endif
