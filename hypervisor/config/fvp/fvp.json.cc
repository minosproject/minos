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
			"setup_data": "0x83e00000",
			"vcpu0_affinity": 0,
			"vcpu1_affinity": 1,
			"vcpu2_affinity": 2,
			"vcpu3_affinity": 3,
			"cmdline": "",
			"bit64": 1
		}
	],
	#include "fvp_irq.json.cc"
	#include "fvp_mem.json.cc"

	"others" : {
		"comments": "minos virtualization config json data"
	}
}
