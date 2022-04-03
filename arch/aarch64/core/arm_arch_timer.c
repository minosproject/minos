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
#include <minos/irq.h>
#include <minos/of.h>
#include <asm/reg.h>
#include <minos/platform.h>
#include <asm/aarch64_reg.h>

enum timer_type {
	SEC_PHY_TIMER,
	NONSEC_PHY_TIMER,
	VIRT_TIMER,
	HYP_TIMER,
	TIMER_MAX,
};

static char *timer_name[TIMER_MAX] = {
	"   sec_phy_timer ",
	"nonsec_phy_timer ",
	"      virt_timer ",
	"hypervisor_timer "
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
		write_sysreg32(0, ARM64_CNTSCHED_CTL);
		return;
	}

	deadline = ns_to_ticks(expires);
	write_sysreg64(deadline, ARM64_CNTSCHED_CVAL);
	write_sysreg32(1 << 0, ARM64_CNTSCHED_CTL);
	isb();
}

unsigned long get_sys_ticks(void)
{
	return read_sysreg64(CNTPCT_EL0);
}

unsigned long get_current_time(void)
{
	return ticks_to_ns(read_sysreg64(CNTPCT_EL0) - boot_tick);
}

unsigned long get_sys_time(void)
{
	return ticks_to_ns(read_sysreg64(CNTPCT_EL0));
}

static int __init_text timers_arch_init(void)
{
	int i, ret, from_dt;
	struct armv8_timer_info *info;
	struct device_node *node = NULL;

#ifdef CONFIG_DEVICE_TREE
	node = of_find_node_by_compatible(of_root_node, arm_arch_timer_match_table);
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

		pr_notice("%s : %d\n", timer_name[i], info->irq);
	}

	ret = of_get_u32_array(node, "clock-frequency", &cpu_khz, 1);
	if (cpu_khz > 0) {
		cpu_khz = cpu_khz / 1000;
		from_dt = 1;
	} else {
		cpu_khz = read_sysreg32(CNTFRQ_EL0) / 1000;
		from_dt = 0;
	}

	isb();
	boot_tick = read_sysreg64(CNTPCT_EL0);
	pr_notice("clock freq from %s %d\n", from_dt ? "DTB" : "REG", cpu_khz);
	pr_notice("boot ticks is :0x%x\n", boot_tick);

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
	extern void soft_timer_interrupt(void);

	write_sysreg32(0, ARM64_CNTSCHED_CTL);
	soft_timer_interrupt();

	return 0;
}

static int __init_text timers_init(void)
{
	struct armv8_timer_info *sched_timer_info = NULL;

#ifdef CONFIG_VIRT
	struct armv8_timer_info *info;
	extern int virtual_timer_irq_handler(uint32_t irq, void *data);

	write_sysreg64(0, CNTVOFF_EL2);

	/* el1/el0 can read CNTPCT_EL0 */
	write_sysreg32(1 << 0, CNTHCTL_EL2);

	/* disable hyper and phy timer */
	write_sysreg32(0, CNTP_CTL_EL0);
	write_sysreg32(0, CNTHP_CTL_EL2);
	isb();

	info = &timer_info[VIRT_TIMER];
	if (info->irq) {
		request_irq(info->irq, virtual_timer_irq_handler,
			info->flags & 0xf, "virt timer irq", NULL);
	}

	sched_timer_info = &timer_info[HYP_TIMER];
#else
	sched_timer_info = &timer_info[VIRT_TIMER];
#endif

	ASSERT(sched_timer_info && sched_timer_info->irq);
	request_irq(sched_timer_info->irq,
			timer_interrupt_handler,
			sched_timer_info->flags & 0xf,
			"sched_timer_int", NULL);

	return 0;
}

subsys_initcall_percpu(timers_init);
subsys_initcall(timers_arch_init);
