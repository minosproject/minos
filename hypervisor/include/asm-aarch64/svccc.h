#ifndef _MINOS_ASM_SVCCC_H_
#define _MINOS_ASM_SVCCC_H_

#include <asm/arch.h>

/*
 * bit[31] 	: 0-yielding call 1-fast call
 * bit[30] 	: 0-smc32/hvc32 1-smc64/hvc64
 * bit[29:24]	: service call ranges SVC_STYPE_XX
 * bit[23:16]	: must be zero
 * bit[15:0]	: function number with the range call type
 */
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

#define SVC_RET()		{	\
	return 0;			\
}

#define SVC_RET1(reg, a0)	{	\
	set_reg_value(reg, 0, a0); 	\
	return 0;			\
}

#define SVC_RET2(reg, a0, a1)	{	\
	set_reg_value(reg, 0, a0);	\
	set_reg_value(reg, 1, a1);	\
	return 0;			\
}

#define SVC_RET3(reg, a0, a1, a2)	{	\
	set_reg_value(reg, 0, a0); 	\
	set_reg_value(reg, 1, a1); 	\
	set_reg_value(reg, 2, a2); 	\
	return 0;			\
}

#define SVC_RET4(reg, a0, a1, a2, a3) {	\
	set_reg_value(reg, 0, a0); 	\
	set_reg_value(reg, 1, a1); 	\
	set_reg_value(reg, 2, a2); 	\
	set_reg_value(reg, 3, a3); 	\
	return 0;			\
}

#define HVC_RET0()		 	SVC_RET0()
#define HVC_RET1(reg, a0)		SVC_RET1(reg, a0)
#define HVC_RET2(reg, a0, a1)	 	SVC_RET2(reg, a0, a1)
#define HVC_RET3(reg, a0, a1, a2)	SVC_RET3(reg, a0, a1, a2)
#define HVC_RET4(reg, a0, a1, a2, a3) 	SVC_RET4(reg, a0, a1, a2, a3)

typedef int (*svc_handler_t)(gp_regs *c, uint32_t id, uint64_t *args);

struct svc_desc {
	char *name;
	uint16_t type_start;
	uint16_t type_end;
	svc_handler_t handler;
};

#define DEFINE_SMC_HANDLER(n, start, end, h)	\
	static struct svc_desc __smc_##h __used \
	__section(".__smc_handler") = {	\
		.name = n,	\
		.type_start = start, \
		.type_end = end, \
		.handler = h, \
	}

#define DEFINE_HVC_HANDLER(n, start, end, h)	\
	static struct svc_desc __hvc_##h __used \
	__section(".__hvc_handler") = {	\
		.name = n,	\
		.type_start = start, \
		.type_end = end, \
		.handler = h, \
	}

int do_svc_handler(gp_regs *regs, uint32_t svc_id,
		uint64_t *args, int smc);

static int inline do_smc_handler(gp_regs *regs,
		uint32_t svc_id, uint64_t *args)
{
	return do_svc_handler(regs, svc_id, args, 1);
}

static int inline do_hvc_handler(gp_regs *regs,
		uint32_t svc_id, uint64_t *args)
{
	return do_svc_handler(regs, svc_id, args, 0);
}

#endif
