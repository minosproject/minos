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
#include <asm/processer.h>
#include <minos/platform.h>

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

void udelay(unsigned long us)
{
	unsigned long deadline = get_sys_time() + 1000 *
		(unsigned long)us;

	while (get_sys_time() - deadline < 0);

	dsbsy();
	isb();
}

static int timers_arch_init(void)
{
	int ret, len;
	unsigned long hz = 0;
	struct armv8_timer_info *info;

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
		pr_info("timer %d int is %d flags-0x%x\n",
				len, info->irq, info->flags);
	}

	len = 0;
	ret = of_get_u32_array("/timer", "clock-frequency",
			(uint32_t *)&hz, &len);
	if ((ret == 0) || (len > 0)) {
		cpu_khz = hz / 1000;
		pr_info("get timer clock freq from dt %d\n", cpu_khz);
	} else {
		cpu_khz = read_sysreg32(CNTFRQ_EL0) / 1000;
		pr_info("get timer clock freq from reg %d\n", cpu_khz);
	}

	boot_tick = read_sysreg64(CNTPCT_EL0);
	pr_info("boot_tick:0x%x cpu_khz:%d\n", boot_tick, cpu_khz);

	if (platform->time_init)
		platform->time_init();

	return 0;
}

static int timer_interrupt_handler(uint32_t irq, void *data)
{
	raise_softirq(TIMER_SOFTIRQ);
	write_sysreg32(0, CNTP_CTL_EL0);

	return 0;
}

void sched_tick_disable(void)
{
	write_sysreg32(0, CNTHP_CTL_EL2);
	isb();
}

void sched_tick_enable(unsigned long exp)
{
	unsigned long deadline;

	if (exp == 0) {
		write_sysreg32(0, CNTHP_CTL_EL2);
		return;
	}

	deadline = read_sysreg64(CNTPCT_EL0);
	deadline += ns_to_ticks(exp);
	write_sysreg64(deadline, CNTHP_CVAL_EL2);
	write_sysreg32(1 << 0, CNTHP_CTL_EL2);
	isb();
}

static int sched_timer_handler(uint32_t irq, void *data)
{
	unsigned long next_evt;

	/* disable timer to avoid interrupt */
	write_sysreg32(0, CNTHP_CTL_EL2);

	next_evt = sched_tick_handler((unsigned long)data);
	sched_tick_enable(next_evt);

	return 0;
}

static int virtual_timer_irq_handler(uint32_t irq, void *data)
{
	/* if the current vcpu is idle, disable the vtimer */
	if (current_vcpu->is_idle) {
		write_sysreg32(0, CNTV_CTL_EL0);
		return -ENOENT;
	}

	return send_virq_to_vcpu(current_vcpu, irq);
}

static int timers_init(void)
{
	struct armv8_timer_info *info;

	write_sysreg64(0, CNTVOFF_EL2);

	/* el1/el0 can read CNTPCT_EL0 */
	write_sysreg32(1 << 0, CNTHCTL_EL2);

	/* disable hyper and phy timer */
	write_sysreg32(0, CNTP_CTL_EL0);
	write_sysreg32(0, CNTHP_CTL_EL2);
	isb();

	/* used for sched ticks */
	info = &timer_info[HYP_TIMER];
	if (info->irq) {
		request_irq(info->irq, sched_timer_handler,
			info->flags & 0xf, "hyp timer int", NULL);
	}

	info = &timer_info[NONSEC_PHY_TIMER];
	if (info->irq) {
		request_irq(info->irq, timer_interrupt_handler,
			info->flags & 0xf, "nonsec timer int", NULL);
	}

	info = &timer_info[VIRT_TIMER];
	if (info->irq) {
		request_irq(info->irq, virtual_timer_irq_handler,
			IRQ_FLAGS_VCPU | (info->flags & 0xf),
			"virt timer irq", NULL);
	}

	return 0;
}

module_initcall_percpu(timers_init);
arch_initcall(timers_arch_init);
