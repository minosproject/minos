#define MEM_SHARED		0
#define MEM_IO			1
#define MEM_NORMAL		2

#define VMID_HOST		65535

"memtags": [
	{
		"mem_base": "0xd0000000",
		"mem_end": "0xd1bfffff",
		"enable": 1,
		"type": MEM_IO,
		"vmid": 0,
		"name": "32M internal register"
		/* d1d00000 is for gic */
	},
	{
		"mem_base": "0xe8000000",
		"mem_end": "0xe8ffffff",
		"enable": 1,
		"type": MEM_IO,
		"vmid": 0,
		"name": "pcie region"
	},
	{
		"mem_base": "0x00200000",
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
		"vmid": VMID_HOST,
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
