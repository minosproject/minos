/*
 * Created by Le Min 2017/12/12
 */

#include <core/string.h>
#include <asm/cpu.h>

int boot_main(void)
{
	if (get_cpu_id() != 0)
		panic("cpu is not cpu0");

	init_mem_block();
	init_vms();
}
