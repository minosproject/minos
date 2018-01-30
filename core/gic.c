#include <core/types.h>
#include <core/spinlock.h>
#include <core/gic.h>
#include <core/smp.h>

int gic_local_init(void)
{
	int cpuid;

	cpuid = get_cpu_id();

	/*
	 * if boot cpu do not complete are setting
	 * other cpu will wait here util boot cpu
	 * setup the ARE bit, at that time, the GICR
	 * base address has been init.
	 *
	 * in no-secure access just enable the ARE_NS bit
	 */
	sync_are_in_gicd(GICD_CTLR_ARE_NS, cpuid);

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
		enable_gicd(GICD_CTLR_ENABLE_GRP1A | GICD_CTLR_ENABLE_GRP1);
		enable_private_int(15);
	} else	{
		/*
		 * this routine is enable NO.15 sgi interrupt
		 * for waiting boot cpu wake up other cpu
		 */
		set_private_int_priority(15, 14 << 4);
		enable_private_int(15);
	}

	return 0;
}

int gic_global_init(void)
{
	gic_gicd_global_init();
	gic_gicr_global_init();
}
