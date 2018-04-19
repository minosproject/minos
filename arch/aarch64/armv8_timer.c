#include <mvisor/mvisor.h>
#include <mvisor/time.h>
#include <mvisor/init.h>
#include <mvisor/io.h>
#include <mvisor/stdlib.h>
#include <mvisor/softirq.h>

#define CNTCR		0x000
#define CNTSR		0x004
#define CNTCV_L		0x008
#define CNTCV_H		0x00c
#define CNTFID0		0x020

uint32_t cpu_khz = 0;
uint64_t boot_tick = 0;

void *refclk_cnt_base = (void *)0x2a430000;

void arch_enable_timer(unsigned long expires)
{
	uint64_t deadline;

	if (expires == 0) {
		write_sysreg32(0, CNTHP_CTL_EL2);
		return;
	}

	deadline = ns_to_ticks(expires) + boot_tick;
	pr_info("the deadline is %d\n", deadline);
	write_sysreg64(deadline, CNTHP_CVAL_EL2);
	write_sysreg32(1 << 0, CNTHP_CTL_EL2);
	isb();
}

unsigned long get_sys_time()
{
	uint64_t ticks;

	ticks = read_sysreg64(CNTPCT_EL0);

	return ticks_to_ns(ticks);
}

static int timers_arch_init(void)
{
	if (!cpu_khz)
		cpu_khz = ioread32(refclk_cnt_base + CNTFID0);

	if (!boot_tick)
		boot_tick = read_sysreg64(CNTPCT_EL0);

	iowrite32(refclk_cnt_base + CNTCR, 1);

	return 0;
}

static int timer_interrupt_handler(uint32_t irq, void *data)
{
	raise_softirq(TIMER_SOFTIRQ);
	write_sysreg32(0, CNTHP_CTL_EL2);

	return 0;
}


static int vtimer_interrupt_handler(uint32_t irq, void *data)
{
	return 0;
}

static int timers_init(void)
{
	write_sysreg64(0, CNTVOFF_EL2);
	write_sysreg32(1 << 2, CNTHCTL_EL2);
	write_sysreg32(0, CNTP_CTL_EL0);
	write_sysreg32(0, CNTHP_CTL_EL2);
	isb();

	request_irq(HYP_TIMER_INT, timer_interrupt_handler, NULL);
	//request_irq(PHYS_TIMER_NONSEC_INT, timer_interrupt_handler, NULL);
	request_irq(VIRT_TIMER_INT, vtimer_interrupt_handler, NULL);

	return 0;
}

subsys_initcall_percpu(timers_init);
arch_initcall(timers_arch_init);
