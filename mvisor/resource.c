#include <mvisor/mvisor.h>
#include <mvisor/resource.h>
#include <mvisor/irq.h>

extern unsigned char __vmm_memory_resource_start;
extern unsigned char __vmm_memory_resource_end;
extern unsigned char __vmm_irq_resource_start;
extern unsigned char __vmm_irq_resource_end;

struct vmm_resource_table {
	unsigned long start_addr;
	unsigned long end_addr;
	int type;
	unsigned int entry_size;
};

struct vmm_resource_table tables[] = {
	{
		.start_addr = (unsigned long)&__vmm_memory_resource_start,
		.end_addr = (unsigned long)&__vmm_memory_resource_end,
		.type = VMM_RESOURCE_TYPE_MEMORY,
		.entry_size = sizeof(struct memory_resource),
	},
	{
		.start_addr = (unsigned long)&__vmm_irq_resource_start,
		.end_addr = (unsigned long)&__vmm_irq_resource_end,
		.type = VMM_RESOURCE_TYPE_IRQ,
		.entry_size = sizeof(struct irq_resource),
	},
};

static int __vmm_parse_resource(unsigned long start,
		unsigned long end, int type, unsigned int entry_size)
{
	int i, ret;
	int entry_count;
	void *resource;

	if ((start == 0) || (end == 0) ||
		(start >= end) || (entry_size == 0)) {
		pr_error("resource table not vaild\n");
		return -EINVAL;
	}

	if (type >= VMM_RESOURCE_TYPE_UNKNOWN) {
		pr_error("Invaild resoruce type\n");
		return -EINVAL;
	}

	entry_count = (end - start) / entry_size;

	for (i = 0; i < entry_count; i++) {
		resource = (void *)start;

		switch (type) {
		case VMM_RESOURCE_TYPE_MEMORY:
			ret = vmm_register_memory_region(resource);
			break;

		case VMM_RESOURCE_TYPE_IRQ:
			ret = vmm_register_irq_entry(resource);
			break;

		default:
			pr_error("resource can not be handled\n");
			return -EINVAL;
		}

		start += entry_size;
	}

	return ret;
}

int vmm_parse_resource(void)
{
	int i;
	size_t size;
	struct vmm_resource_table *table;

	size = sizeof(tables) / sizeof(tables[0]);

	for (i = 0; i < size; i++) {
		table = &tables[i];

		__vmm_parse_resource(table->start_addr, table->end_addr,
				table->type, table->entry_size);
	}
}
