#include "espressobin_config.h"
{
	"version": "0.0.1",
	"platform": "armv8-fvp",

	"vmtags": [
	{
			"vmid": 0,
			"name": "espressobin",
			"type": "linux",
			"nr_vcpu": 1,
			"entry": "0x80080000",
			"setup_data": "0x83e00000",
			"vcpu0_affinity": 0,
			"vcpu1_affinity": 1,
			"vcpu2_affinity": 2,
			"vcpu3_affinity": 3,
			"bit64": 1
		}
	],
	#include "irq.json.cc"
	#include "mem.json.cc"

	"others" : {
		"comments": "minos virtualization config json data"
	}
}
