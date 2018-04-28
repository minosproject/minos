#include <mvisor/mvisor.h>
#include <mvisor/mmu.h>
#include <mvisor/mm.h>
#include <mvisor/module.h>

static struct mmu_chip *mmu_chip;

int mmu_map_guest_memory(unsigned long page_table_base, unsigned long phy_base,
		unsigned long vir_base, size_t size, int type)
{
	if (!mmu_chip->map_guest_memory)
		return -EINVAL;

	return mmu_chip->map_guest_memory(page_table_base,
			phy_base, vir_base, size, type);
}

unsigned long mmu_alloc_guest_pt(void)
{
	if (!mmu_chip->alloc_guest_pt)
		return -EINVAL;

	return mmu_chip->alloc_guest_pt();
}

int mmu_map_host_memory(unsigned long vir,
		unsigned long phy, size_t size, int type)
{
	if (!mmu_chip->map_host_memory)
		return -EINVAL;

	return mmu_chip->map_host_memory(vir, phy, size, type);
}

int io_remap(unsigned long vir, unsigned long phy, size_t size)
{
	if (!mmu_chip->map_host_memory)
		return -EINVAL;

	return mmu_chip->map_host_memory(vir, phy, size, MEM_TYPE_IO);
}

int mvisor_mmu_init(void)
{
	char *chip_name = CONFIG_MMU_CHIP_NAME;

	mmu_chip = (struct mmu_chip *)mvisor_get_module_pdata(chip_name,
			MVISOR_MODULE_NAME_MMU);
	if (!mmu_chip)
		panic("can not find the mmuchip for system\n");

	return 0;
}
