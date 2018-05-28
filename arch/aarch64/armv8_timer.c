#include <minos/minos.h>
#include <minos/time.h>
#include <minos/init.h>
#include <minos/io.h>
#include <minos/stdlib.h>
#include <minos/softirq.h>
#include <asm/vtimer.h>
#include <minos/sched.h>
#include <virt/vmodule.h>
#include <minos/irq.h>

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
	io_remap(0x2a810000, 0x2a810000, 64 * 1024);
	io_remap(0x2a430000, 0x2a430000, 64 * 1024);

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
	struct vcpu *vcpu = current_vcpu;
	struct vtimer_context *c = (struct vtimer_context *)
		get_vmodule_data_by_id(vcpu, vtimer_vmodule_id);
	struct vtimer *vtimer = &c->virt_timer;

	vtimer->cnt_ctl = read_sysreg32(CNTV_CTL_EL0);
	write_sysreg32(vtimer->cnt_ctl | CNT_CTL_IMASK, CNTV_CTL_EL0);
	send_virq_to_vcpu(vcpu, vtimer->virq);

	return 0;
}

static int timers_init(void)
{
	write_sysreg64(0, CNTVOFF_EL2);
	write_sysreg32(1 << 0, CNTHCTL_EL2);
	write_sysreg32(0, CNTP_CTL_EL0);
	write_sysreg32(0, CNTHP_CTL_EL2);
	isb();

	request_irq(HYP_TIMER_INT, timer_interrupt_handler,
			0, "hyp timer int", NULL);

	request_irq(PHYS_TIMER_NONSEC_INT, timer_interrupt_handler,
			0, "nonsec timer int", NULL);

	request_irq(VIRT_TIMER_INT, vtimer_interrupt_handler,
			0, "virt timer int", NULL);

	return 0;
}

module_initcall_percpu(timers_init);
arch_initcall(timers_arch_init);
