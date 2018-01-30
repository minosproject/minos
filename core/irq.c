#include <core/core.h>
#include <core/irq.h>
#include <config/vm_config.h>
#include <core/vcpu.h>

static struct vmm_irq irq_table[CONFIG_MAX_IRQ];

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
	memset((char *)irq_table, 0, sizeof(vmm_irq) * CONFIG_MAX_IRQ);

	for (i = 0; i < size; i++) {
		config = &configs[i];
		if (SPI_OFFSET(config->hno) >= CONFIG_MAX_IRQ) {
			pr_error("Find an invalid irq %d\n", config->hno);
			continue;
		}

		vcpu = vmm_get_vcpu(config->vmid, config->affinity);
		if (!vcpu) {
			pr_error("Vcpu:%d is not exist for this vm\n", config->affinity);
			continue;
		}

		vmm_irq = &irq_table[SPI_OFFSET(config->hno)];
		vmm_irq->vno = config->vno;
		vmm_irq->hno = config->hno;
		vmm_irq->vmid = config->vmid;
		vmm_irq->affinity_vcpu = config->affinity;
		vmm_irq->affinity_pcpu = vmm_get_pcpu_id(vcpu);
		strncpy(vmm_irq->name, config->name,
			MIN(strlen(config->name), MAX_IRQ_NAME_SIZE - 1));
	}
}

void vmm_irqs_init(void)
{
	parse_all_irqs();
}
