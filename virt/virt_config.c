#include <virt/virt.h>
#include <virt/virq.h>
#include <virt/vmm.h>

static void parse_memtags(void)
{
	int i;
	size_t size = mv_config->nr_memtag;
	struct memtag *memtags = mv_config->memtags;

	for (i = 0; i < size; i++)
		register_memory_region(&memtags[i]);

}

static void parse_virqs(void)
{
	int i;
	size_t size = mv_config->nr_virqtag;
	struct virqtag *virqtags = mv_config->virqtags;

	for (i = 0; i < size; i++)
		register_virq(&virqtags[i]);
}

void parse_vm_config(void)
{
	parse_memtags();
	parse_virqs();
}
