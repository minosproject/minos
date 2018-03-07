#include <mvisor/mvisor.h>
#include <mvisor/mmu.h>
#include <mvisor/mm.h>
#include <mvisor/module.h>

static struct mmu_chip *mmu_chip;

int mmu_map_memory(unsigned long page_table_base, unsigned long phy_base,
		unsigned long vir_base, size_t size, int type)
{
	if (!mmu_chip->map_memory)
		return -EINVAL;

	return mmu_chip->map_memory(page_table_base,
			phy_base, size, vir_base, type);
}

unsigned long mmu_alloc_page_table(void)
{
	if (!mmu_chip->alloc_page_table)
		return 0;

	return mmu_chip->alloc_page_table();
}

int vmm_mmu_init(void)
{
	char *chip_name = CONFIG_MMU_CHIP_NAME;

	mmu_chip = (struct mmu_chip *)vmm_get_module_pdata(chip_name,
			VMM_MODULE_NAME_MMU);
	if (!mmu_chip)
		panic("can not find the mmuchip for system\n");

	return 0;
}
