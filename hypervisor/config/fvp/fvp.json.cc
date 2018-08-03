#include "fvp_config.h"
{
	"version": "0.0.1",
	"platform": "armv8-fvp",

	"vmtags": [
	{
			"vmid": 0,
			"name": "linux-01",
			"type": "linux",
			"nr_vcpu": 1,
#ifdef CONFIG_PLATFORM_FVP
			"entry": "0x80080000",
			"setup_data": "0x83e00000",
#else
			"entry": "0xc0080000"
			"setup_data": "0xc3e00000",
#endif
			"vcpu0_affinity": 0,
			"vcpu1_affinity": 1,
			"vcpu2_affinity": 2,
			"vcpu3_affinity": 3,
			"bit64": 1
		}
#if 0
		{
			"vmid": 1,
			"name": "os1",
			"type": "other",
			"nr_vcpu": 4,
			"entry": "0x84000000",
			"vcpu0_affinity": 0,
			"vcpu1_affinity": 1,
			"vcpu2_affinity": 2,
			"vcpu3_affinity": 3,
			"setup_data": "0xd0000000",
			"mmu_on": 1
		}
#endif
	],
	#include "fvp_virq.json.cc"
	#include "fvp_mem.json.cc"

	"others" : {
		"comments": "minos virtualization config json data"
	}
}
