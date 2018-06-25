#include "fvp_config.h"
{
	"version": "0.0.1",
	"platform": "armv8-fvp",

	"vmtags": [
	{
			"vmid": 0,
			"name": "linux-01",
			"type": "linux",
			"nr_vcpu": 4,
			"entry": "0x80080000",
			"vcpu0_affinity": 0,
			"vcpu1_affinity": 1,
			"vcpu2_affinity": 2,
			"vcpu3_affinity": 3,
			"setup_data": "0x83e00000",
			"mmu_on": 1
		},
		{
			"vmid": 1,
			"name": "os1",
			"type": "other",
			"nr_vcpu": 4,
			"entry": "0x84000000",
			"vcpu0_affinity": 1,
			"vcpu1_affinity": 0,
			"vcpu2_affinity": 2,
			"vcpu3_affinity": 3,
			"setup_data": "0xd0000000",
			"mmu_on": 1
		}
	],
	#include "fvp_virq.json.cc"
	#include "fvp_mem.json.cc"

	"others" : {
		"comments": "minos virtualization config json data"
	}
}
