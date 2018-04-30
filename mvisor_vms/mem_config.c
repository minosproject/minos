#include <mvisor/resource.h>
#include <mvisor/init.h>

/*
 * mem_type : 0 - normal memory
 * 	      1 - device memory
 * 	      2 - shared memory
 */
static struct memory_resource mem_regions[] __memory_resource = {
	{
		.mem_base = 0x90000000,
		.mem_end  = 0xA0000000,
		.type = 0,
		.vmid = 0,
		.name = "dram",
	},

	{
		.mem_base = 0xa0000000,
		.mem_end  = 0xb0000000,
		.type = 0,
		.vmid = 1,
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
		.mem_base = 0x1C090000,
		.mem_end  = 0x1C0A0000,
		.type = 1,
		.vmid = 1,
		.name = "uart0",
	},

	{
		.mem_base = 0x1c1f0000,
		.mem_end = 0x1c200000,
		.type = 1,
		.vmid = 0,
		.name = "color_lcd",
	},

	{
		.mem_base = 0x1c010000,
		.mem_end = 0x1c020000,
		.type = 1,
		.vmid = 0,
		.name = "ve-sys",
	},
	{
		.mem_base = 0x1c110000,
		.mem_end = 0x1c120000,
		.type = 1,
		.vmid = 0,
		.name = "timer",
	},
};
