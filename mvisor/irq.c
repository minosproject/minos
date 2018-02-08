#include <mvisor/mvisor.h>
#include <mvisor/irq.h>
#include <config/vm_config.h>
#include <mvisor/vcpu.h>
#include <mvisor/mm.h>
#include <config/config.h>
#include <mvisor/device_id.h>
#include <mvisor/module.h>

uint32_t irq_nums;
static struct vmm_irq **irq_table;
typedef uint32_t (*get_nr_t)(void);

struct irq_chip *irq_chip;

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
		if ((config->hno >= irq_nums) || (config->hno < NR_LOCAL_IRQS)) {
			pr_error("Find an invalid irq %d\n", config->hno);
			continue;
		}

		vcpu = vmm_get_vcpu(config->vmid, config->affinity);
		if (!vcpu) {
			pr_error("Vcpu:%d is not exist for this vm\n", config->affinity);
			continue;
		}

		vmm_irq = irq_table[config->hno];
		if (!vmm_irq) {
			pr_error("irq %d not been allocated\n", config->hno);
			continue;
		}

		vmm_irq->vno = config->vno;
		vmm_irq->hno = config->hno;
		vmm_irq->vmid = config->vmid;
		vmm_irq->affinity_vcpu = config->affinity;
		vmm_irq->affinity_pcpu = vmm_get_pcpu_id(vcpu);
		strncpy(vmm_irq->name, config->name,
			MIN(strlen(config->name), MAX_IRQ_NAME_SIZE - 1));
		vmm_irq->flags |= config->type;
	}
}

static void vmm_set_up_irqs(void)
{
	int i;
	struct vmm_irq *vmm_irq;

	for (i = 0; i < irq_nums; i++) {
		vmm_irq = irq_table[i];
		if (!vmm_irq)
			continue;

		//gic_set_irq_affinity(vmm_irq, vmm_irq->affinity_pcpu);
		//gic_set_irq_type(vmm_irq, vmm_irq->flags & IRQ_TYPE_MASK);
		//gic_unmask_irq(vmm_irq);
	}
}

int vmm_alloc_irqs(uint32_t start,
		uint32_t end, unsigned long flags)
{
	int i;
	struct vmm_irq *irq;

	if (start >= irq_nums)
		return -EINVAL;

	for (i = start; i <= end; i++) {
		if (i >= irq_nums) {
			pr_error("Irq num not supported %d\n", i);
			break;
		}

		irq = irq_table[i];
		if (irq)
			pr_warning("Irq %d has been allocted\n");
		else
			irq = (struct vmm_irq *)vmm_malloc(sizeof(struct vmm_irq));

		memset((char *)irq, 0, sizeof(struct vmm_irq));
		irq->hno = i;
		if (flags)
			irq->flags |= IRQ_OWNER_PERCPU;
	}

	return 0;
}

int vmm_irq_init(void)
{
	uint32_t size;
	char *chip_name = CONFIG_IRQ_CHIP_NAME;

	irq_chip = (struct irq_chip *)vmm_get_module_pdata(chip_name,
			VMM_MODULE_NAME_IRQCHIP);
	if (!irq_chip)
		panic("can not find the irqchip for system\n");

	irq_nums = irq_chip->irq_num;

	size = irq_nums * sizeof(struct vmm_irq *);
	irq_table = (struct vmm_irq **)vmm_malloc(size);
	if (!irq_table)
		panic("No more memory for irq tables\n");

	memset((char *)irq_table, 0, size);

	/*
	 * now init the irqchip, and in the irq chip
	 * the chip driver need to alloc the irq it
	 * need used in the ssystem
	 */
	if (irq_chip->init)
		irq_chip->init();

	parse_all_irqs();

	return 0;
}

int vmm_irq_secondary_init(void)
{
	if (irq_chip)
		irq_chip->secondary_init();

	return 0;
}
