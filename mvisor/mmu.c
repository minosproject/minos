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

static int check_mmuchip(struct module_id *module)
{
	return (!(strcmp(module->name, CONFIG_MMU_CHIP_NAME)));
}

int mvisor_mmu_init(void)
{
	extern unsigned char __mvisor_mmuchip_start;
	extern unsigned char __mvisor_mmuchip_end;
	unsigned long s, e;

	s = (unsigned long)&__mvisor_mmuchip_start;
	e = (unsigned long)&__mvisor_mmuchip_end;

	mmu_chip = (struct mmu_chip *)get_module_data(s, e, check_mmuchip);
	if (!mmu_chip)
		panic("can not find the mmuchip for system\n");

	return 0;
}
