#ifndef _MVISOR_ASM_VTIMER_H_
#define _MVISOR_ASM_VTIMER_H_

#include <mvisor/timer.h>

struct vcpu;

#define CNTCR		0x000
#define CNTSR		0x004
#define CNTCV_L		0x008
#define CNTCV_H		0x00c
#define CNTFID0		0x020

#define CNTVCT_LO	0x08
#define CNTVCT_HI	0x0c
#define CNTFRQ		0x10
#define CNTP_TVAL	0x28
#define CNTP_CTL	0x2c
#define CNTV_TVAL	0x38
#define CNTV_CTL	0x3c

#define CNT_CTL_ISTATUS		(1 << 2)
#define CNT_CTL_IMASK		(1 << 1)
#define CNT_CTL_ENABLE		(1 << 0)

struct vtimer {
	struct vcpu *vcpu;
	struct timer_list timer;
	int virq;
	uint32_t cnt_ctl;
	uint32_t cnt_cval;
};

struct vtimer_context {
	struct vtimer phy_timer;
	struct vtimer virt_timer;
	unsigned long offset;
};

extern int vtimer_module_id;

int vtimer_sysreg_simulation(vcpu_regs *reg, uint32_t esr_value);

#endif
