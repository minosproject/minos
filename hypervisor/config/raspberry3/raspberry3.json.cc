#include "raspberry3_config.h"
{
	"version": "0.0.1",
	"platform": "raspberry model-3b",

	"vmtags": [
	{
			"vmid": 0,
			"name": "linux-01",
			"type": "linux",
			"nr_vcpu": 2,
			"entry": "0x00080000",
			"setup_data": "0x03e00000",
			"vcpu0_affinity": 0,
			"vcpu1_affinity": 1,
			"vcpu2_affinity": 2,
			"vcpu3_affinity": 3,
			"cmdline": "",
			"bit64": 1
		}
	],
	#include "rpi3_irq.json.cc"
	#include "rpi3_mem.json.cc"

	"others" : {
		"comments": "minos virtualization config json data"
	}
}
