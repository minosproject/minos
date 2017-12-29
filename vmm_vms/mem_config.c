#include <config/vm_config.h>

/*
 * mem_type : 0 - normal memory
 * 	      1 - device memory
 * 	      2 - shared memory
 */
static struct vmm_memory_region mem_regions[] = {
	{
		.mem_base = 0x90000000,
		.mem_end  = 0xA0000000,
		.type = 0,
		.vmid = 0,
		.name = "dram",
	},

	{
		.mem_base = 0x1C090000,
		.mem_end  = 0x1C0A0000,
		.type = 1,
		.vmid = 0,
		.name = "uart0",
	},

	{
		.mem_base = 0xa0000000,
		.mem_end = 0xa4000000,
		.type = 2,
		.vmid = 0xffff,
		.name = "shared01",
	},

	{
		.mem_base = 0x1c1f0000,
		.mem_end = 0x1c200000,
		.type = 1,
		.vmid = 0,
		.name = "color_lcd",
	},
};

uint32_t get_mem_config_size(void)
{
	return (sizeof(mem_regions) /
		sizeof(struct vmm_memory_region));
}

void *get_mem_config_data(void)
{
	return (void *)mem_regions;
}
