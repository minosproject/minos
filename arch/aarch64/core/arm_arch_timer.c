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
#include <asm/io.h>
#include <minos/stdlib.h>
#include <minos/softirq.h>
#include <minos/sched.h>
#include <minos/vmodule.h>
#include <minos/irq.h>
#include <minos/of.h>
#include <asm/processer.h>
#include <minos/platform.h>

enum timer_type {
	SEC_PHY_TIMER,
	NONSEC_PHY_TIMER,
	VIRT_TIMER,
	HYP_TIMER,
	TIMER_MAX,
};

static char *timer_name[TIMER_MAX] = {
	"sec_phy_timer",
	"nonsec_phy_timer",
	"virt_timer",
	"hypervisor_timer"
};

struct armv8_timer_info {
	uint32_t irq;
	unsigned long flags;
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

	deadline = ns_to_ticks(expires);
	write_sysreg64(deadline, CNTP_CVAL_EL0);
	write_sysreg32(1 << 0, CNTP_CTL_EL0);
	isb();
}

unsigned long get_sys_ticks(void)
{
	unsigned long ticks;

	ticks = read_sysreg64(CNTPCT_EL0);
	dsb();

	return ticks;
}

unsigned long get_current_time(void)
{
	uint64_t ticks;

	ticks = read_sysreg64(CNTPCT_EL0) - boot_tick;
	dsb();

	return ticks_to_ns(ticks);
}

unsigned long get_sys_time(void)
{
	uint64_t ticks;

	ticks = read_sysreg64(CNTPCT_EL0);
	dsb();

	return ticks_to_ns(ticks);
}

static int timers_arch_init(void)
{
	int i, ret;
	struct armv8_timer_info *info;
	struct device_node *node = NULL;
	char *comp[3] = {
		"arm,armv8-timer",
		"arm,armv7-timer",
		NULL,
	};

#ifdef CONFIG_DEVICE_TREE
	node = of_find_node_by_compatible(hv_node, comp);
#endif
	if (!node) {
		pr_err("can not find arm-arch-timer\n");
		return -EINVAL;
	}

	for (i = 0; i < TIMER_MAX; i++) {
		info = &timer_info[i];
		ret = get_device_irq_index(node, &info->irq, &info->flags, i);
		if (ret) {
			pr_err("error found in arm timer config\n");
			return -ENOENT;
		}

		pr_info("%s : irq-%d flags-0x%x\n", timer_name[i],
				info->irq, info->flags);
	}

	ret = of_get_u32_array(node, "clock-frequency", &cpu_khz, 1);
	if (cpu_khz > 0) {
		cpu_khz = cpu_khz / 1000;
		pr_info("get timer clock freq from dt %d\n", cpu_khz);
	} else {
		cpu_khz = read_sysreg32(CNTFRQ_EL0) / 1000;
		pr_info("get timer clock freq from reg %d\n", cpu_khz);
	}

	boot_tick = read_sysreg64(CNTPCT_EL0);
	pr_info("boot ticks is :0x%x\n", boot_tick);

	if (platform->time_init)
		platform->time_init();

#ifdef CONFIG_VIRT
	extern int arch_vtimer_init(uint32_t virtual_irq, uint32_t phy_irq);
	arch_vtimer_init(timer_info[VIRT_TIMER].irq,
			timer_info[NONSEC_PHY_TIMER].irq);
#endif

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
	/* disable timer to avoid interrupt */
	write_sysreg32(0, CNTHP_CTL_EL2);
	wmb();

	(void)sched_tick_handler((unsigned long)data);

	return 0;
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

#ifdef CONFIG_VIRT
	extern int virtual_timer_irq_handler(uint32_t irq, void *data);

	info = &timer_info[VIRT_TIMER];
	if (info->irq) {
		request_irq(info->irq, virtual_timer_irq_handler,
			info->flags & 0xf, "virt timer irq", NULL);
	}
#endif

	return 0;
}

subsys_initcall_percpu(timers_init);
subsys_initcall(timers_arch_init);
