#include <mvisor/mvisor.h>
#include <mvisor/irq.h>
#include <config/vm_config.h>
#include <mvisor/vcpu.h>
#include <mvisor/mm.h>
#include <config/config.h>
#include <mvisor/device_id.h>
#include <mvisor/module.h>
#include <mvisor/resource.h>

uint32_t irq_nums;
static struct vmm_irq **irq_table;
typedef uint32_t (*get_nr_t)(void);

static struct irq_chip *irq_chip;

int vmm_register_irq_entry(void *res)
{
	struct irq_resource *config;
	struct vmm_irq *vmm_irq;
	vcpu_t *vcpu;

	if (res == NULL)
		return -EINVAL;

	config = (struct irq_resource *)res;
	if ((config->hno >= irq_nums) || (config->hno < NR_LOCAL_IRQS)) {
		pr_error("Find an invalid irq %d\n", config->hno);
		return -EINVAL;
	}

	vcpu = vmm_get_vcpu(config->vmid, config->affinity);
	if (!vcpu) {
		pr_error("Vcpu:%d is not exist for this vm\n", config->affinity);
		return -EINVAL;;
	}

	vmm_irq = irq_table[config->hno];
	if (!vmm_irq) {
		pr_error("irq %d not been allocated\n", config->hno);
		return -ENOMEM;
	}

	vmm_irq->vno = config->vno;
	vmm_irq->hno = config->hno;
	vmm_irq->vmid = config->vmid;
	if (vmm_irq->vmid == 0xffffffff)
		vmm_irq->flag |= IRQ_FLAG_OWNER_VMM;

	vmm_irq->affinity_vcpu = config->affinity;
	vmm_irq->affinity_pcpu = get_pcpu_id(vcpu);
	strncpy(vmm_irq->name, config->name,
		MIN(strlen(config->name), MAX_IRQ_NAME_SIZE - 1));
	vmm_irq->flags |= config->type;

	return 0;
}

void vmm_setup_irqs(void)
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

static int send_virtual_irq(vcpu_t *vcpu, uint32_t v, uint32_t h)
{
	int ret;
	void *context = NULL;

	if (vcpu)
		context = get_vcpu_module_data(vcpu, VMM_MODULE_NAME_IRQCHIP);

	/*
	 * just to change some register's settings, ture
	 * virq will trigger when return to guest vm, if
	 * the virq is not send successful this time then
	 * add it to the pending list for next time
	 */
	ret = irq_chip->send_virtual_irq(context, v, h);

	return 0;
}

static int do_handle_guest_irq(struct vmm_irq *vmm_irq, vcpu_t *vcpu)
{
	vm_t *vm;
	vcpu_t *vcpu_o;

	vm = get_vm_by_id(vmm_irq->vmid);
	if (!vm) {
		pr_error("Invaild vm for this irq %d\n", vmm_irq->hno);
		return -EINVAL;
	}

	vcpu_o = get_vcpu_in_vm(vm, vmm_irq->affinity_vcpu);
	if (vcpu_o == NULL) {
		pr_error("Invaild vcpu for this irq %d\n", vmm_irq->hno);
		return -EINVAL;
	}

	/*
	 * do send the virtual irq to the guest vm
	 */
	send_virtual_irq(vcpu_o, vmm_irq->vno, vmm_irq->hno);

	if ()

	return 0;
}

static int do_handle_vmm_irq(struct vmm_irq *vmm_irq, vcpu_t *vcpu)
{
	uint32_t cpuid = get_cpu_id();

	if (cpuid != vmm_irq->affinity_pcpu) {
		pr_info("irq %d do not belong tho this cpu\n", vmm_irq->hno);
		return -EINVAL;
	}

	if (!vmm_irq->irq_handler) {
		pr_error("Irq is not register by VMM\n");
		return -EINVAL;
	}

	return vmm_irq->irq_handler(vmm_irq->hno, vmm_irq->pdata);
}

static int do_spi_int(uint32_t irq, vcpu_t *vcpu)
{
	struct vmm_irq *vmm_irq;
	int ret;

	vmm_irq = irq_table[irq];
	if (vmm_irq == NULL) {
		pr_error("irq is not register\n");
		goto out;
	}

	if (vmm_irq->flags & IRQ_FLAG_OWNER_VMM)
		ret = do_handle_vmm_irq(vmm_irq, vcpu);
	else
		ret = do_handle_guest_irq(vmm_irq, vcpu);

	if (irq_chip->handle_spi_int)
		irq_chip->handle_spi_int(vmm_irq, vcpu);

	return ret;
}

static int do_sgi_int(uint32_t irq, vcpu_t *vcpu)
{
	return 0;
}

static int do_ppi_int(uint32_t irq, vcpu_t *vcpu)
{
	return 0;
}

static int do_lpi_int(uint32_t irq, vcpu_t *vcpu)
{
	return 0;
}

static int do_special_int(uint32_t irq, vcpu_t *vcpu)
{
	return 0;
}

static int do_lpi_int(uint32_t irq, vcpu_t *vcpu)
{
	return 0;
}

static int do_bad_int(uint32_t irq, vcpu_t *vcpu)
{
	return 0;
}

int do_irq_handler(vcpu_t *vcpu)
{
	uint32_t irq;
	int ret;
	struct vmm_irq *vmm_irq;
	int type = IRQ_TYPE_SPI;

	if (!irq_chip)
		panic("irq_chip is Null when irq is triggered\n");

	irq = irq_chip->get_pending_irq();

	if (irq_chip->get_irq_type(irq))
		type = irq_chip->get_irq_type(irq);

	/*
	 * TBD - here we need deactive the irq
	 * for arm write the ICC_EOIR1_EL1 register
	 */

	switch (type) {
	case IRQ_TYPE_SGI:
		ret = do_sgi_int(irq, vcpu);
		break;
	case IRQ_TYPE_PPI:
		ret = do_ppi_int(irq, vcpu);
		break;
	case IRQ_TYPE_SPI:
		ret = do_spi_int(irq, vcpu);
		break;
	case IRQ_TYPE_LPI:
		ret = do_lpi_int(irq, vcpu);
		break;
	case IRQ_TYPE_SPECIAL:
		ret = do_special_int(irq, vcpu);
		break;
	case IRQ_TYPE_BAD:
		ret = do_bad_int(irq, vcpu);
		break;
	default:
		break;
	}

	irq_chip->eoi_irq(irq);
	return ret;
}


static int irq_exit_from_guest(vcpu_t *vcpu, void *data)
{
	/*
	 * here we update the states of the irq state
	 * which the vcpu is handles, since this is running
	 * on percpu and hanlde per_vcpu's data so do not
	 * need spinlock
	 */
	struct vcpu_irq *vcpu_irq;
	uint32_t set_bit;
	int status;
	void *context;
	struct irq_struct *irq_struct = &vcpu->irq_struct;

	context = get_vcpu_module_data(vcpu, VMM_MODULE_NAME_IRQCHIP);

	for_each_set_bit(set_bit, &irq_struct->irq_bitmap,
			CONFIG_VCPU_MAX_ACTIVE_IRQS) {
		vcpu_irq = (struct vcpu_irq *)&irq_struct->vcpu_irqs[set_bit];
		status = irq_chip->get_virq_state(vcpu_irq, context);

		/*
		 * the virq has been handled by the VCPU
		 */
		if (status == VIRQ_STATE_INACTIVE) {
			clear_bit(&irq_struct->irq_bitmap, set_bit);
			irq_struct->count--;
			if (irq_struct->count < 0) {
				pr_error("irq count is error\n");
				break;
			}

			vcpu_irq->h_intno = 0;
			vcpu_irq->v_intno = 0;
			vcpu_irq->state = VIRQ_STATE_INACTIVE;
		}
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

	if (!irq_chip->get_pending_irq)
		panic("No function to get irq nr\n");

	vmm_register_hook(irq_exit_from_guest,
			NULL, VMM_HOOK_TYPE_EXIT_FROM_GUEST);

	return 0;
}

int vmm_irq_secondary_init(void)
{
	if (irq_chip)
		irq_chip->secondary_init();

	return 0;
}
