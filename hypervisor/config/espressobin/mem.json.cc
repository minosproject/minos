#define MEM_SHARED		0
#define MEM_IO			1
#define MEM_NORMAL		2

#define VMID_HOST		65535

"memtags": [{
		"mem_base": "0xd0000000",
		"mem_end": "0x2000000",
		"enable": 1,
		"type": MEM_IO,
		"vmid": 0,
		"name": "32M internal register"
	},
	{
		"mem_base": "0x00000000",
		"mem_end": "0x0fffffff",
		"sectype": "S/NS",
		"enable": 1,
		"type": MEM_NORMAL,
		"vmid": 0,
		"name": "vm0 dram"
	},
	{
		"mem_base": "0x10000000",
		"mem_end": "0x3bffffff",
		"enable": 1,
		"type": MEM_NORMAL,
		"vmid": 65535,
		"name": "dram"
	},
	{
		"mem_base": "0x3c000000",
		"mem_end": "0x3fffffff",
		"enable": 1,
		"type": MEM_NORMAL,
		"vmid": 65535,
		"name": "dram"
	}
],
