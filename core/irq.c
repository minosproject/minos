#include <core/core.h>
#include <core/irq.h>
#include <config/vm_config.h>
#include <core/vcpu.h>
#include <core/gic.h>
#include <core/mem_block.h>
#include <asm/armv8.h>

static uint32_t max_irq_nums;
static struct vmm_irq **irq_table;

void enable_irq(void)
{
	arch_enable_irq();
}

void disable_irq(void)
{
	arch_disable_irq();
}

static void parse_all_irqs(void)
{
	int i;
	uint32_t size;
	struct irq_config *configs;
	struct irq_config *config;
	struct vmm_irq *vmm_irq;
	vcpu_t *vcpu;

	size = get_irq_config_size();
	configs = (struct irq_config *)get_irq_config_table();

	for (i = 0; i < size; i++) {
		config = &configs[i];
		if (SPI_OFFSET(config->hno) >= max_irq_nums) {
			pr_error("Find an invalid irq %d\n", config->hno);
			continue;
		}

		vcpu = vmm_get_vcpu(config->vmid, config->affinity);
		if (!vcpu) {
			pr_error("Vcpu:%d is not exist for this vm\n", config->affinity);
			continue;
		}

		vmm_irq = (struct vmm_irq *)vmm_malloc(sizeof(struct vmm_irq));
		if (!vmm_irq)
			panic("No more memory for vmm irq\n");

		vmm_irq->vno = config->vno;
		vmm_irq->hno = config->hno;
		vmm_irq->vmid = config->vmid;
		vmm_irq->affinity_vcpu = config->affinity;
		vmm_irq->affinity_pcpu = vmm_get_pcpu_id(vcpu);
		strncpy(vmm_irq->name, config->name,
			MIN(strlen(config->name), MAX_IRQ_NAME_SIZE - 1));
		vmm_irq->flags = 0;
		irq_table[SPI_OFFSET(config->hno)] = vmm_irq;
	}
}

static void vmm_set_up_irqs(void)
{
	int i;
	struct vmm_irq *vmm_irq;

	for (i = 0; i < max_irq_nums; i++) {
		vmm_irq = irq_table[i];
		if (!vmm_irq)
			continue;

		gic_set_irq_affinity(vmm_irq, vmm_irq->affinity_pcpu);
		gic_set_irq_type(vmm_irq, vmm_irq->flags & IRQ_TYPE_MASK);
		gic_unmask_irq(vmm_irq);
	}
}

void vmm_irqs_init(void)
{
	uint32_t size;

	max_irq_nums = gic_get_line_num();
	if (max_irq_nums == 0)
		max_irq_nums = 1024;

	size = max_irq_nums * sizeof(struct vmm_irq *);
	irq_table = (struct vmm_irq **)vmm_malloc(size);
	if (!irq_table)
		panic("No more memory for irq tables\n");

	memset((char *)irq_table, 0, size);

	parse_all_irqs();
	vmm_set_up_irqs();
}
