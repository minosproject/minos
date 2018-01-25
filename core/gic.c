#include <core/types.h>
#include <core/spinlock.h>
#include <core/gic.h>
#include <asm/cpu.h>

int gic_local_init(void)
{
	int cpuid;

	cpuid = get_cpu_id();

	sync_are_in_gicd((1 << 4) | (1 << 5), cpuid);
	wakeup_gicr();

	/*
	 * set the sec block of private int
	 * private int inclue SGI and PPI which are
	 * banked for each PES.
	 * SGIs : 0 - 15
	 * PPIs : 16 - 31
	 */
	set_private_int_sec_block(1);

	if (cpuid == 0) {
		/*
		* boot cpu init routin, set the shared int also
		* called SPIs's sec block
		* SPIs : 32 -1019
		*/
		set_spi_int_sec_all(1);
		enable_gicd(1 << 1);
	} else	{
		set_private_int_priority(15, 14 << 4);
		enable_private_int(15);
	}

	return 0;
}
