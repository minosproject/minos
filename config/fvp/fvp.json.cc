#include "fvp_config.h"
{
	"version": "0.0.1",
	"platform": "armv8-fvp",
	"mem_base": "0x80000000",
	"size": "64MB",

	"vmtags": [{
			"vmid": 0,
			"name": "linux-01",
			"type": "linux",
			"nr_vcpu": 4,
			"entry": "0x90000000",
			"vcpu0_affinity": 0,
			"vcpu1_affinity": 1,
			"vcpu2_affinity": 2,
			"vcpu3_affinity": 3,
			"mmu_on": 1
		},
		{
			"vmid": 1,
			"name": "os1",
			"type": "other",
			"nr_vcpu": 4,
			"entry": "0xa0000000",
			"vcpu0_affinity": 1,
			"vcpu1_affinity": 0,
			"vcpu2_affinity": 2,
			"vcpu3_affinity": 3,
			"mmu_on": 1
		}
	],
	#include "fvp_irq.json.cc"
	#include "fvp_mem.json.cc"

	"others" : {
		"comments": "mvisor config json data"
	}
}
