#include <minos/minos.h>
#include <minos/mmu.h>
#include <minos/mm.h>
#include <virt/vmodule.h>

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

static int check_mmuchip(struct module_id *vmodule)
{
	return (!(strcmp(vmodule->name, CONFIG_MMU_CHIP_NAME)));
}

int mmu_init(void)
{
	extern unsigned char __mmuchip_start;
	extern unsigned char __mmuchip_end;
	unsigned long s, e;

	s = (unsigned long)&__mmuchip_start;
	e = (unsigned long)&__mmuchip_end;

	mmu_chip = (struct mmu_chip *)get_vmodule_data(s, e, check_mmuchip);
	if (!mmu_chip)
		panic("can not find the mmuchip for system\n");

	return 0;
}
