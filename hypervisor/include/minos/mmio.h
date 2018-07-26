#ifndef _MINOS_MMIO_H_
#define _MINOS_MMIO_H_

#include <minos/list.h>
#include <minos/types.h>
#include <minos/vcpu.h>

struct mmio_info {
	unsigned long write_value;
	unsigned long address;
};

struct mmio_ops {
	int (*read)(gp_regs *regs, unsigned long address,
			unsigned long *read_value);
	int (*write)(gp_regs *regs, unsigned long address,
			unsigned long *write_value);
	int (*check)(gp_regs *regs, unsigned long address);
};

#define MMIO_HANDLER_NAME_SIZE	(32)

struct mmio_handler {
	char name[MMIO_HANDLER_NAME_SIZE];
	struct mmio_ops *ops;
	struct list_head list;
};

int do_mmio_emulation(gp_regs *regs, int write,
		unsigned long address, unsigned long *value);
int register_mmio_emulation_handler(char *name, struct mmio_ops *ops);

#endif
