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

#include <asm/aarch64_helper.h>
#include <asm/exception.h>
#include <minos/minos.h>
#include <minos/smp.h>
#include <asm/processer.h>
#include <minos/sched.h>
#include <minos/irq.h>
#include <asm/svccc.h>

extern unsigned char __sync_desc_start;
extern unsigned char __sync_desc_end;

extern void sync_from_lower_EL_handler(gp_regs *data);

static struct sync_desc *sync_descs[MAX_SYNC_TYPE] __align_cache_line;

void bad_mode(void)
{
	panic("Bad error received\n");
}

void sync_from_current_EL_handler(gp_regs *data)
{
	uint32_t esr_value;
	uint32_t ec_type;
	struct sync_desc *ec;

	esr_value = read_esr_el2();
	ec_type = (esr_value & 0xfc000000) >> 26;
	ec = sync_descs[ec_type];
	pr_err("SError_from_current_EL_handler : 0x%x\n", ec_type);
	if (ec != NULL)
		ec->handler(data, esr_value);

	__panic(data, "system hang due to sync error in EL2\n");
}

void sync_c_handler(gp_regs *regs)
{
	if (taken_from_guest(regs)) {
#ifdef CONFIG_VIRT
		sync_from_lower_EL_handler(regs);
#endif
	} else {
		sync_from_current_EL_handler(regs);
	}
}

static int aarch64_sync_init(void)
{
	struct sync_desc *desc;

	section_for_each_item(__sync_desc_start, __sync_desc_end, desc) {
		sync_descs[desc->type] = desc;
	}

	return 0;
}

arch_initcall(aarch64_sync_init);
