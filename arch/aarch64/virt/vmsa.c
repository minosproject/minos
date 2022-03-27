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
#include <asm/arch.h>
#include <asm/aarch64_reg.h>

struct aa64mmfr0 {
	uint64_t pa_range : 4;
	uint64_t asid : 4;
	uint64_t big_end : 4;
	uint64_t sns_mem : 4;
	uint64_t big_end_el0 : 4;
	uint64_t t_gran_16k : 4;
	uint64_t t_gran_64k : 4;
	uint64_t t_gran_4k : 4;
	uint64_t res : 32;
};

static int __init_text el2_stage2_init(void)
{
	/*
	 * now just support arm fvp, TBD to support more
	 * platform
	 */
	uint64_t value, dcache, icache;
	struct aa64mmfr0 *aa64mmfr0;

	value = read_id_aa64mmfr0_el1();
	aa64mmfr0 = (struct aa64mmfr0 *)&value;
	pr_info("aa64mmfr0: pa_range:0x%x t_gran_16k:0x%x t_gran_64k"
		 ":0x%x t_gran_4k:0x%x\n", aa64mmfr0->pa_range,
		 aa64mmfr0->t_gran_16k, aa64mmfr0->t_gran_64k,
		 aa64mmfr0->t_gran_4k);

	value = read_sysreg(CTR_EL0);
	dcache = 4 << ((value & 0xf0000) >> 16);
	icache = 4 << ((value & 0xf));
	pr_info("dcache_line_size:%d ichache_line_size:%d\n", dcache, icache);

	return 0;
}
early_initcall(el2_stage2_init);
