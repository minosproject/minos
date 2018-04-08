#include <mvisor/mvisor.h>
#include <mvisor/irq.h>
#include <config/vm_config.h>
#include <mvisor/vcpu.h>
#include <mvisor/mm.h>
#include <config/config.h>
#include <mvisor/device_id.h>
#include <mvisor/module.h>
#include <mvisor/resource.h>
#include <mvisor/sched.h>

uint32_t irq_nums;
static struct vmm_irq **irq_table;
typedef uint32_t (*get_nr_t)(void);

static struct irq_chip *irq_chip;

static uint32_t virq_to_irq(uint32_t virq)
{
	int i;
	struct vmm_irq *vmm_irq;

	for (i = 0; i < irq_nums; i++) {
		vmm_irq = irq_table[i];
		if (!vmm_irq)
			continue;

		if (vmm_irq->vno == virq)
			return vmm_irq->hno;
	}

	return BAD_IRQ;
}

int vmm_register_irq_entry(void *res)
{
	struct irq_resource *config;
	struct vmm_irq *vmm_irq;
	vcpu_t *vcpu;

	if (res == NULL)
		return -EINVAL;

	config = (struct irq_resource *)res;
	if ((config->hno >= irq_nums)) {
		pr_error("Find an invalid irq %d\n", config->hno);
		return -EINVAL;
	}

	vmm_irq = irq_table[config->hno];
	if (!vmm_irq) {
		pr_error("irq %d not been allocated\n", config->hno);
		return -ENOMEM;
	}

	if (config->vmid == 0xffff) {
		pr_info("irq %d is for vmm\n", config->hno);
		vmm_irq->flags |= IRQ_FLAG_OWNER_VMM;
		strncpy(vmm_irq->name, config->name,
			MIN(strlen(config->name), MAX_IRQ_NAME_SIZE - 1));
		return 0;
	}

	vcpu = get_vcpu_by_id(config->vmid, config->affinity);
	if (!vcpu) {
		pr_error("Vcpu:%d is not exist for this vm\n", config->affinity);
		return -EINVAL;;
	}

	vmm_irq->vno = config->vno;
	vmm_irq->hno = config->hno;
	vmm_irq->vmid = config->vmid;

	vmm_irq->affinity_vcpu = config->affinity;
	vmm_irq->affinity_pcpu = get_pcpu_id(vcpu);
	strncpy(vmm_irq->name, config->name,
		MIN(strlen(config->name), MAX_IRQ_NAME_SIZE - 1));
	vmm_irq->flags |= config->type;

	return 0;
}

void __irq_enable(uint32_t irq, int enable)
{
	unsigned long flag;
	struct vmm_irq *vmm_irq;

	if (irq >= irq_nums)
		return;

	vmm_irq = irq_table[irq];
	if (!vmm_irq)
		return;

	spin_lock_irqsave(&vmm_irq->lock, flag);

	if (enable) {
		if (vmm_irq->flags & IRQ_FLAG_STATUS_MASK ==
				IRQ_FLAG_STATUS_MASKED)
			goto out;

		irq_chip->irq_unmask(irq);
		vmm_irq->flags &= ~IRQ_FLAG_STATUS_MASK;
		vmm_irq->flags |= IRQ_FLAG_STATUS_UNMASKED;
	} else {
		if (vmm_irq->flags & IRQ_FLAG_STATUS_MASK ==
				IRQ_FLAG_STATUS_UNMASKED)
			goto out;

		irq_chip->irq_mask(irq);
		vmm_irq->flags &= ~IRQ_FLAG_STATUS_MASK;
		vmm_irq->flags |= IRQ_FLAG_STATUS_MASKED;
	}

out:
	spin_unlock_irqrestore(&vmm_irq->lock, flag);
}

void __virq_enable(uint32_t virq, int enable)
{
	uint32_t irq;

	irq = virq_to_irq(virq);
	if (irq == BAD_IRQ)
		return;

	__irq_enable(irq, enable);
}

void vmm_setup_irqs(void)
{
	int i;
	struct vmm_irq *vmm_irq;

	for (i = 0; i < irq_nums; i++) {
		vmm_irq = irq_table[i];
		if (!vmm_irq)
			continue;

		if (vmm_irq->flags & IRQ_FLAG_AFFINITY_VCPU) {
			irq_chip->irq_set_type(vmm_irq->hno,
					vmm_irq->flags & IRQ_FLAG_TYPE_MASK);
			irq_chip->irq_set_affinity(vmm_irq->hno,
					vmm_irq->affinity_pcpu);
		}
	}
}

int vmm_alloc_irqs(uint32_t start, uint32_t end, enum irq_type type)
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
		irq_table[i] = irq;
		spin_lock_init(&irq->lock);

		if (type == IRQ_TYPE_SPI)
			irq->flags |= IRQ_FLAG_AFFINITY_VCPU;
	}

	return 0;
}

void send_sgi(uint32_t sgi, int cpu)
{
	cpumask_t mask;

	if ((cpu < 0) || (cpu >= CONFIG_NR_CPUS))
		return;

	if (sgi >= 16)
		return;

	cpumask_clear(&mask);
	cpumask_set_cpu(cpu, &mask);

	irq_chip->send_sgi(sgi, SGI_TO_LIST, &mask);
}

static int __send_virq(vcpu_t *vcpu, uint32_t vno, uint32_t hno, int hw)
{
	struct irq_struct *irq_struct;
	struct virq *virq;
	int index;

	irq_struct = &vcpu->irq_struct;

	spin_lock(&irq_struct->lock);

	index = find_first_zero_bit(irq_struct->irq_bitmap,
			CONFIG_VCPU_MAX_ACTIVE_IRQS);
	if (index == CONFIG_VCPU_MAX_ACTIVE_IRQS) {
		/*
		 * no empty resource to handle this virtual irq
		 * need to drop it ? TBD
		 */
		pr_error("Can not send this virq now\n");
		return -EAGAIN;
	}

	virq = &irq_struct->virqs[index];
	virq->h_intno = hno;
	virq->v_intno = vno;
	virq->hw = hw;
	virq->id = index;
	virq->state = VIRQ_STATE_OFFLINE;
	set_bit(index, irq_struct->irq_bitmap);
	list_add_tail(&irq_struct->pending_list, &virq->list);
	irq_struct->irq_pending++;

	spin_unlock(&irq_struct->lock);

	return 0;
}

int _send_virq(vcpu_t *vcpu, uint32_t virq, uint32_t hirq, int hw)
{
	int ret = 0;
	vcpu_t *vcpu_sender = current_vcpu();

	ret = __send_virq(vcpu, virq, hirq, hw);
	if (!ret)
		goto out;

	if (vcpu_sender->pcpu_affinity != vcpu->pcpu_affinity) {
		/*
		 * if the sender and the target are not
		 * the same pcpu, then send a hw sgi to the
		 * pcpu to do the sched work
		 */
		send_sgi(CONFIG_VMM_RESCHED_IRQ, vcpu_sender->pcpu_affinity);
	} else {
		/*
		 * if the sender and the target are the same
		 * pcpu, but the vcpu is not the same, just
		 * update the sched information to decide whether
		 * need to reshched
		 */
		if (vcpu != vcpu_sender)
			sched_vcpu(vcpu, /* SCHED_REASON_IRQ_PENDING*/ 0);
	}

out:
	return ret;
}

static int do_handle_guest_irq(struct vmm_irq *vmm_irq)
{
	vm_t *vm;
	vcpu_t *vcpu;

	vm = get_vm_by_id(vmm_irq->vmid);
	if (!vm) {
		pr_error("Invaild vm for this irq %d\n", vmm_irq->hno);
		return -EINVAL;
	}

	vcpu = get_vcpu_in_vm(vm, vmm_irq->affinity_vcpu);
	if (vcpu == NULL) {
		pr_error("Invaild vcpu for this irq %d\n", vmm_irq->hno);
		return -EINVAL;
	}

	return _send_virq(vcpu, vmm_irq->vno, vmm_irq->hno, 1);
}

int send_virq_hw(uint32_t vmid, uint32_t virq, uint32_t hirq)
{
	struct vmm_irq *vmm_irq;
	vcpu_t *vcpu;

	if (hirq >= irq_nums)
		return -EINVAL;

	vmm_irq = irq_table[hirq];
	if (!vmm_irq)
		return -ENOENT;

	if (vmid != vmm_irq->vmid)
		return -EINVAL;

	vcpu = get_vcpu_by_id(vmid, vmm_irq->affinity_vcpu);
	if (!vcpu)
		return -ENOENT;

	return _send_virq(vcpu, virq, hirq, 1);
}

int send_virq(uint32_t vmid, uint32_t virq)
{
	/*
	 * default all the virq do not attached to
	 * the hardware irq will send to the vcpu0
	 * of a vm
	 */
	return _send_virq(get_vcpu_by_id(vmid, 0), virq, 0, 0);
}

void send_vsgi(vcpu_t *sender, uint32_t sgi, cpumask_t *cpumask)
{
	int cpu;
	vcpu_t *vcpu;
	vm_t *vm = sender->vm;

	for_each_set_bit(cpu, cpumask->bits, vm->vcpu_nr) {
		vcpu = vm->vcpus[cpu];
		__send_virq(vcpu, sgi, 0, 0);
	}
}

static int do_handle_vmm_irq(struct vmm_irq *vmm_irq)
{
	uint32_t cpuid = get_cpu_id();
	int ret;

	if (cpuid != vmm_irq->affinity_pcpu) {
		pr_info("irq %d do not belong tho this cpu\n", vmm_irq->hno);
		ret =  -EINVAL;
		goto out;
	}

	if (!vmm_irq->handler) {
		pr_error("Irq is not register by VMM\n");
		ret = -EINVAL;
		goto out;
	}

	ret = vmm_irq->handler(vmm_irq->hno, vmm_irq->pdata);
	if (ret)
		pr_error("handle irq:%d fail in vmm\n", vmm_irq->hno);

out:
	irq_chip->irq_dir(vmm_irq->hno);

	return ret;
}

static inline int do_spi_int(struct vmm_irq *vmm_irq)
{
	int ret;

	if (vmm_irq->flags & IRQ_FLAG_OWNER_VMM)
		ret = do_handle_vmm_irq(vmm_irq);
	else
		ret = do_handle_guest_irq(vmm_irq);

	return ret;
}

static inline int do_sgi_int(struct vmm_irq *vmm_irq)
{
	return do_handle_vmm_irq(vmm_irq);
}

static int do_ppi_int(struct vmm_irq *vmm_irq)
{
	return 0;
}

static int do_lpi_int(uint32_t irq)
{
	return 0;
}

static int do_special_int(uint32_t irq)
{
	return 0;
}

static int do_bad_int(uint32_t irq)
{
	return 0;
}

static int __do_irq_handler(uint32_t irq, int type)
{
	int ret = 0;
	struct vmm_irq *vmm_irq = NULL;

	if ((type == IRQ_TYPE_SGI) || (type == IRQ_TYPE_PPI) ||
			(type == IRQ_TYPE_SPI)) {
		vmm_irq = irq_table[irq];
		if (vmm_irq == NULL) {
			pr_error("irq is not register\n");
			return -EINVAL;
		}
	}

	switch (type) {
	case IRQ_TYPE_SGI:
		ret = do_sgi_int(vmm_irq);
		break;
	case IRQ_TYPE_PPI:
		ret = do_ppi_int(vmm_irq);
		break;
	case IRQ_TYPE_SPI:
		ret = do_spi_int(vmm_irq);
		break;
	case IRQ_TYPE_LPI:
		ret = do_lpi_int(irq);
		break;
	case IRQ_TYPE_SPECIAL:
		ret = do_special_int(irq);
		break;
	case IRQ_TYPE_BAD:
		ret = do_bad_int(irq);
		break;
	default:
		break;
	}

	return ret;
}

int do_irq_handler(void)
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
	 * to drop the priority
	 */
	irq_chip->irq_eoi(irq);

	return __do_irq_handler(irq, type);
}

int request_irq(uint32_t irq, irq_handle_t handler, void *data)
{
	struct vmm_irq *vmm_irq;
	unsigned long flag;

	if ((!handler) || (irq >= irq_nums))
		return -EINVAL;

	vmm_irq = irq_table[irq];
	if (!vmm_irq)
		return -ENOENT;

	/*
	 * whether the irq is belong to vmm
	 */
	if (!(vmm_irq->flags & IRQ_FLAG_OWNER_MASK))
		return -ENOENT;

	spin_lock_irqsave(&vmm_irq->lock, flag);
	vmm_irq->handler = handler;
	vmm_irq->pdata = data;
	spin_unlock_irqrestore(&vmm_irq->lock, flag);

	irq_unmask(irq);

	return 0;
}

static void irq_enter_to_guest(vcpu_t *vcpu, void *data)
{
	/*
	 * here we send the real virq to the vcpu
	 * before it enter to guest
	 */
	struct virq *virq;
	struct irq_struct *irq_struct = &vcpu->irq_struct;

	spin_lock(&irq_struct->lock);

	list_for_each_entry(virq, &irq_struct->pending_list, list) {
		if (virq->state != VIRQ_STATE_OFFLINE)
			pr_debug("something was wrong with this irq %d\n", virq->id);

		virq->state = VIRQ_STATE_PENDING;
		irq_chip->send_virq(virq);
		list_del(&virq->list);
		irq_struct->irq_pending--;
	}

	spin_unlock(&irq_struct->lock);
}

static void irq_exit_from_guest(vcpu_t *vcpu, void *data)
{
	/*
	 * here we update the states of the irq state
	 * which the vcpu is handles, since this is running
	 * on percpu and hanlde per_vcpu's data so do not
	 * need spinlock
	 */
	struct virq *virq;
	uint32_t set_bit;
	int status;
	struct irq_struct *irq_struct = &vcpu->irq_struct;

	spin_lock(&irq_struct->lock);

	for_each_set_bit(set_bit, irq_struct->irq_bitmap,
			CONFIG_VCPU_MAX_ACTIVE_IRQS) {
		virq = (struct virq *)&irq_struct->virqs[set_bit];

		if (virq->state == VIRQ_STATE_OFFLINE)
			continue;

		status = irq_chip->get_virq_state(virq);

		/*
		 * the virq has been handled by the VCPU
		 */
		if (status == VIRQ_STATE_INACTIVE) {
			irq_struct->count--;
			if (irq_struct->count < 0) {
				pr_error("irq count is error\n");
				break;
			}

			virq->h_intno = 0;
			virq->v_intno = 0;
			virq->state = VIRQ_STATE_INACTIVE;
			clear_bit(set_bit, irq_struct->irq_bitmap);
		}
	}

	spin_unlock(&irq_struct->lock);
}

void vcpu_irq_struct_init(struct irq_struct *irq_struct)
{
	int i;
	struct virq *virq;

	if (!irq_struct)
		return;

	irq_struct->count = 0;
	spin_lock_init(&irq_struct->lock);
	init_list(&irq_struct->pending_list);
	bitmap_clear(irq_struct->irq_bitmap, 0, CONFIG_VCPU_MAX_ACTIVE_IRQS);
	irq_struct->irq_pending = 0;

	for (i = 0; i < CONFIG_VCPU_MAX_ACTIVE_IRQS; i++) {
		virq = &irq_struct->virqs[i];
		virq->h_intno = 0;
		virq->v_intno = 0;
		virq->state = VIRQ_STATE_INACTIVE;
		virq->id = i;
		virq->hw = 0;
		init_list(&virq->list);
	}
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
	vmm_register_hook(irq_enter_to_guest,
			NULL, VMM_HOOK_TYPE_ENTER_TO_GUEST);

	return 0;
}

int vmm_irq_secondary_init(void)
{
	if (irq_chip)
		irq_chip->secondary_init();

	return 0;
}
