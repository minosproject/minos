/*
 * Copyright (C) 2018 Min Le (lemin9538@gmail.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <minos/minos.h>
#include <minos/time.h>
#include <minos/init.h>
#include <minos/io.h>
#include <minos/stdlib.h>
#include <minos/softirq.h>
#include <asm/vtimer.h>
#include <minos/sched.h>
#include <minos/vmodule.h>
#include <minos/irq.h>
#include <minos/virq.h>
#include <asm/of.h>

enum timer_type {
	SEC_PHY_TIMER,
	NONSEC_PHY_TIMER,
	VIRT_TIMER,
	HYP_TIMER,
	TIMER_MAX,
};

struct armv8_timer_info {
	uint32_t type;
	uint32_t irq;
	uint32_t flags;
};

static struct armv8_timer_info timer_info[TIMER_MAX];

uint32_t cpu_khz = 0;
uint64_t boot_tick = 0;

void *refclk_cnt_base = (void *)0x2a430000;

extern unsigned long sched_tick_handler(unsigned long data);

void arch_enable_timer(unsigned long expires)
{
	uint64_t deadline;

	if (expires == 0) {
		write_sysreg32(0, CNTP_CTL_EL0);
		return;
	}

	deadline = ns_to_ticks(expires) + boot_tick;
	write_sysreg64(deadline, CNTP_CVAL_EL0);
	write_sysreg32(1 << 0, CNTP_CTL_EL0);
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
	io_remap(0x2a430000, 0x2a430000, 64 * 1024);

	if (!cpu_khz)
		cpu_khz = ioread32(refclk_cnt_base + CNTFID0) / 1000;
	if (!boot_tick)
		boot_tick = read_sysreg64(CNTPCT_EL0);

	pr_info("boot_tick:0x%x cpu_khz:%d\n", boot_tick, cpu_khz);

	/* enable the counter */
	iowrite32(refclk_cnt_base + CNTCR, 1);

	return 0;
}

static int timer_interrupt_handler(uint32_t irq, void *data)
{
	raise_softirq(TIMER_SOFTIRQ);
	write_sysreg32(0, CNTP_CTL_EL0);

	return 0;
}

static int sched_timer_handler(uint32_t irq, void *data)
{
	unsigned long next_evt;
	unsigned long exp;

	/* disable timer to avoid interrupt */
	write_sysreg32(0, CNTHP_CTL_EL2);

	next_evt = sched_tick_handler((unsigned long)data);
	if (next_evt) {
		exp = read_sysreg64(CNTPCT_EL0);
		exp += ns_to_ticks(next_evt);
		exp += boot_tick;

		write_sysreg64(exp, CNTHP_CVAL_EL2);
		write_sysreg32(1 << 0, CNTHP_CTL_EL2);
		isb();
	}

	return 0;
}

void sched_tick_disable(void)
{
	write_sysreg32(0, CNTHP_CTL_EL2);
}

void sched_tick_enable(unsigned long exp)
{
	unsigned long deadline;

	deadline = read_sysreg64(CNTPCT_EL0);
	deadline += ns_to_ticks(exp);
	deadline += boot_tick;

	write_sysreg64(deadline, CNTHP_CVAL_EL2);
	write_sysreg32(1 << 0, CNTHP_CTL_EL2);
	isb();
}

static int virtual_timer_irq_handler(uint32_t irq, void *data)
{
	return send_hirq_to_vcpu(current_vcpu, irq);
}

static int timers_init(void)
{
	int len = 0, ret;
	struct armv8_timer_info *info;

	write_sysreg64(0, CNTVOFF_EL2);

	/* el1/el0 can read CNTPCT_EL0 */
	write_sysreg32(1 << 0, CNTHCTL_EL2);

	/* disable hyper and phy timer */
	write_sysreg32(0, CNTP_CTL_EL0);
	write_sysreg32(0, CNTHP_CTL_EL2);
	isb();

	memset((void *)timer_info, 0, sizeof(timer_info));
	ret = of_get_u32_array("/timer", "interrupts",
			(uint32_t *)timer_info, &len);
	if (ret || (len == 0))
		panic("no arm gen timer found in dtb\n");

	for (len = 0; len < TIMER_MAX; len++) {
		info = &timer_info[len];
		if ((info->type != 1) || (info->irq == 0)) {
			pr_warn("timer int not a ppi %d\n", info->irq);
			continue;
		}

		info->irq += 16;
	}

	/* used for sched ticks */
	info = &timer_info[HYP_TIMER];
	if (info->irq) {
		request_irq(info->irq, sched_timer_handler,
			info->type & 0xff, "hyp timer int", NULL);
	}

	info = &timer_info[NONSEC_PHY_TIMER];
	if (info->irq) {
		request_irq(info->irq, timer_interrupt_handler,
			info->type & 0xff, "nonsec timer int", NULL);
	}

	info = &timer_info[VIRT_TIMER];
	if (info->irq) {
		request_irq(info->irq, virtual_timer_irq_handler,
			IRQ_FLAGS_VCPU | (info->type & 0xff),
			"virt timer irq", NULL);
	}

	return 0;
}

module_initcall_percpu(timers_init);
arch_initcall(timers_arch_init);
