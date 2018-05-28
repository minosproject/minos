#include <minos/minos.h>
#include <minos/irq.h>
#include <virt/virq.h>
#include <minos/sched.h>
#include <minos/minos_config.h>
#include <virt/virq.h>

enum virq_domain_type {
	VIRQ_DOMAIN_SGI = 0,
	VIRQ_DOMAIN_PPI,
	VIRQ_DOMAIN_SPI,
	VIRQ_DOMAIN_LPI,
	VIRQ_DOMAIN_SPECIAL,
	VIRQ_DOMAIN_VIRTUAL,
	VIRQ_DOMAIN_MAX,
};

struct virq_desc {
	int8_t hw;
	uint8_t enable;
	uint16_t vno;
	uint16_t hno;
	uint16_t vmid;
	uint16_t vcpu_id;
} __packed__;

struct virq_domain {
	int type;
	uint32_t start;
	uint32_t end;
	struct virq_desc **table;
};

static struct virq_domain *virq_domains[VIRQ_DOMAIN_MAX];

struct virq_domain *get_virq_domain(uint32_t vno)
{
	int i;
	struct virq_domain *d;

	for (i = 0; i < VIRQ_DOMAIN_MAX; i++) {
		d = virq_domains[i];
		if (!d)
			continue;

		if ((vno >= d->start) && (vno <= d->end))
				return d;
	}

	return NULL;
}

struct virq_desc *get_virq_desc(uint32_t virq)
{
	struct virq_domain *d;

	d = get_virq_domain(virq);
	if (!d)
		return NULL;

	return d->table[virq - d->start];
}

static int __send_virq(struct vcpu *vcpu,
		uint32_t vno, uint32_t hno, int hw)
{
	struct virq_struct *virq_struct;
	struct virq *virq;
	int index;

	virq_struct = &vcpu->virq_struct;

	spin_lock(&virq_struct->lock);

	/*
	 * The following cases are considered software programming
	 * errors and result in UNPREDICTABLE behavior:
	 *
	 * • Having a List register entry with ICH_LR<n>_EL2.HW= 1
	 *   which is associated with a physical interrupt, inactive
	 *   state or in pending state in the List registers if the
	 *   Distributor does not have the corresponding physical
	 *   interrupt in either the active state or the active and
	 *   pending state.
	 * • If ICC_CTLR_EL1.EOImode == 0 or ICC_CTLR_EL3.EOImode_EL3 == 0
	 *   then either:
	 *   — Having an active interrupt in the List registers with a priorit
	 *   that is not set in the corresponding Active Priorities Register.
	 *   — Having two interrupts in the List registers in the active stat
	 *   with the same preemption priority.>
	 * • Having two or more interrupts with the same pINTID in the Lis
	 *   registers for a single virtual CPU interface.
	 */
	if (hw) {
		for_each_set_bit(index, virq_struct->irq_bitmap,
				CONFIG_VCPU_MAX_ACTIVE_IRQS) {
			virq = &virq_struct->virqs[index];
			if (virq->h_intno == hno) {
				pr_error("vcpu has same pirq:%d in pending/actvie state", hno);
				spin_unlock(&virq_struct->lock);
				return -EAGAIN;
			}
		}
	}

	index = find_first_zero_bit(virq_struct->irq_bitmap,
			CONFIG_VCPU_MAX_ACTIVE_IRQS);
	if (index == CONFIG_VCPU_MAX_ACTIVE_IRQS) {
		/*
		 * no empty resource to handle this virtual irq
		 * need to drop it ? TBD
		 */
		pr_error("Can not send this virq now\n");
		spin_unlock(&virq_struct->lock);
		return -EAGAIN;
	}

	virq = &virq_struct->virqs[index];
	virq->h_intno = hno;
	virq->v_intno = vno;
	virq->hw = hw;
	virq->id = index;
	virq->state = VIRQ_STATE_OFFLINE;
	set_bit(index, virq_struct->irq_bitmap);
	list_add_tail(&virq_struct->pending_list, &virq->list);
	virq_struct->pending_count++;

	spin_unlock(&virq_struct->lock);

	return 0;
}

int _send_virq(struct vcpu *vcpu, uint32_t virq, uint32_t hirq, int hw)
{
	int ret = 0;
	struct task *sender = current_task;
	struct task *rec = vcpu->task;

	ret = __send_virq(vcpu, virq, hirq, hw);
	if (ret)
		goto out;

	if (sender && (sender->affinity != rec->affinity)) {
		/*
		 * if the sender and the target are not
		 * the same pcpu, then send a hw sgi to the
		 * pcpu to do the sched work
		 */
		send_sgi(CONFIG_MINOS_RESCHED_IRQ, rec->affinity);
	} else {
		/*
		 * if the sender and the target are the same
		 * pcpu, but the vcpu is not the same, just
		 * update the sched information to decide whether
		 * need to reshched
		 */
		if (sender != rec)
			sched_task(rec, /* SCHED_REASON_IRQ_PENDING*/ 0);
	}

out:
	return ret;
}

int __virq_enable(uint32_t virq, int enable)
{
	struct virq_domain *d;
	struct virq_desc *desc;
	struct vcpu *vcpu = current_vcpu;
	struct virq_struct *vs;
	struct vcpu *c;

	if (vcpu == NULL) {
		pr_error("can not enable virq in host\n");
		return -ENOENT;
	}

	d = get_virq_domain(virq);
	desc = d->table[virq - d->start];

	if ((d->type == VIRQ_DOMAIN_SGI) ||
			(d->type == VIRQ_DOMAIN_PPI)) {

		vs = &vcpu->virq_struct;
		bitmap_set(vs->local_irq_mask, virq - d->start, 1);
		return 0;
	}

	/*
	 * if the virq is affinity to a hw irq, need to
	 * enable the hw irq too
	 */
	if (desc->hw) {
		if (vcpu->vm->vmid != desc->vmid)
			return -EINVAL;

		c = get_vcpu_by_id(vcpu->vm->vmid, desc->vcpu_id);
		if (!c)
			return -EINVAL;

		/* set the affinity and the type TBD */
		irq_set_affinity(desc->hno, get_vcpu_affinity(c));
	}

	return 0;
}

int send_virq_to_vcpu(struct vcpu *vcpu, uint32_t virq)
{
	struct virq_desc *desc;

	desc = get_virq_desc(virq);
	if (!desc)
		return -ENOENT;

	if (desc->hw) {
		pr_error("virq affinity to hw irq can not call %s\n",
				__func__);
		return -EINVAL;
	}

	return _send_virq(vcpu, virq, 0, 0);
}

int send_virq_to_vm(uint32_t vmid, uint32_t virq)
{
	/*
	 * default all the virq do not attached to
	 * the hardware irq will send to the vcpu0
	 * of a vm
	 */
	return send_virq_to_vcpu(get_vcpu_by_id(vmid, 0), virq);
}

void send_vsgi(struct vcpu *sender, uint32_t sgi, cpumask_t *cpumask)
{
	int cpu;
	struct vcpu *vcpu;
	struct vm *vm = sender->vm;

	for_each_set_bit(cpu, cpumask->bits, vm->vcpu_nr) {
		vcpu = vm->vcpus[cpu];
		_send_virq(vcpu, sgi, 0, 0);
	}
}

void clear_pending_virq(uint32_t irq)
{
	int bit;
	struct virq *virq;
	struct vcpu *vcpu = current_vcpu;
	struct virq_struct *virq_struct = &vcpu->virq_struct;

	/*
	 * this function can only called by the current
	 * running vcpu and excuted on the related pcpu
	 */
	spin_lock(&virq_struct->lock);

	for_each_set_bit(bit, virq_struct->irq_bitmap,
			CONFIG_VCPU_MAX_ACTIVE_IRQS) {
		virq = &virq_struct->virqs[bit];

		if ((virq->v_intno == irq) &&
			(virq->state == VIRQ_STATE_PENDING)) {
			irq_update_virq(virq, VIRQ_ACTION_REMOVE);
			virq_struct->active_count--;
			virq->h_intno = 0;
			virq->v_intno = 0;
			virq->hw = 0;
			virq->state = VIRQ_STATE_INACTIVE;
			clear_bit(bit, virq_struct->irq_bitmap);
		}
	}

	spin_unlock(&virq_struct->lock);
}

static void irq_enter_to_guest(struct task *task, void *data)
{
	/*
	 * here we send the real virq to the vcpu
	 * before it enter to guest
	 */
	struct virq *virq;
	struct vcpu *vcpu = task_to_vcpu(task);
	struct virq_struct *virq_struct = &vcpu->virq_struct;

	spin_lock(&virq_struct->lock);

	list_for_each_entry(virq, &virq_struct->pending_list, list) {
		if (virq->state != VIRQ_STATE_OFFLINE)
			pr_debug("something was wrong with this irq %d\n", virq->id);

		virq->state = VIRQ_STATE_PENDING;
		irq_send_virq(virq);
		list_del(&virq->list);
		virq_struct->pending_count--;
		virq_struct->active_count++;
	}

	spin_unlock(&virq_struct->lock);
}

static void irq_exit_from_guest(struct task *task, void *data)
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
	struct vcpu *vcpu = task_to_vcpu(task);
	struct virq_struct *virq_struct = &vcpu->virq_struct;

	spin_lock(&virq_struct->lock);

	for_each_set_bit(set_bit, virq_struct->irq_bitmap,
			CONFIG_VCPU_MAX_ACTIVE_IRQS) {
		virq = (struct virq *)&virq_struct->virqs[set_bit];

		if (virq->state == VIRQ_STATE_OFFLINE)
			continue;

		status = irq_get_virq_state(virq);

		/*
		 * the virq has been handled by the VCPU
		 */
		if (status == VIRQ_STATE_INACTIVE) {
			virq_struct->active_count--;
			if (virq_struct->active_count < 0) {
				pr_error("irq count is error\n");
				break;
			}

			virq->h_intno = 0;
			virq->v_intno = 0;
			virq->hw = 0;
			virq->state = VIRQ_STATE_INACTIVE;
			clear_bit(set_bit, virq_struct->irq_bitmap);
		}
	}

	spin_unlock(&virq_struct->lock);
}

void vcpu_virq_struct_init(struct virq_struct *virq_struct)
{
	int i;
	struct virq *virq;

	if (!virq_struct)
		return;

	virq_struct->active_count = 0;
	spin_lock_init(&virq_struct->lock);
	init_list(&virq_struct->pending_list);
	bitmap_clear(virq_struct->irq_bitmap, 0, CONFIG_VCPU_MAX_ACTIVE_IRQS);
	virq_struct->pending_count = 0;

	for (i = 0; i < CONFIG_VCPU_MAX_ACTIVE_IRQS; i++) {
		virq = &virq_struct->virqs[i];
		virq->h_intno = 0;
		virq->v_intno = 0;
		virq->state = VIRQ_STATE_INACTIVE;
		virq->id = i;
		virq->hw = 0;
		init_list(&virq->list);
	}
}

int vcpu_has_virq_pending(struct vcpu *vcpu)
{
	return (!!vcpu->virq_struct.pending_count);
}

int vcpu_has_virq_active(struct vcpu *vcpu)
{
	return (!!vcpu->virq_struct.active_count);
}

int vcpu_has_virq(struct vcpu *vcpu)
{
	int active, pending;

	active = !!vcpu->virq_struct.active_count;
	pending = !!vcpu->virq_struct.pending_count;

	return (active || pending);
}

int alloc_virtual_irqs(uint32_t start, uint32_t count, int type)
{
	struct virq_domain *d;
	struct virq_desc **descs;

	if (type >= VIRQ_DOMAIN_MAX)
		return -EINVAL;

	d = virq_domains[type];
	if (!d) {
		d = (struct virq_domain *)malloc(sizeof(struct virq_domain));
		if (!d)
			return -ENOMEM;

		virq_domains[type] = d;
	}

	descs = (struct virq_desc **)malloc(count *
			sizeof(struct virq_desc *));
	if (!descs)
		return -ENOMEM;

	d->start = start;
	d->end = start + count - 1;
	d->table = descs;

	return 0;
}

int guest_irq_handler(uint32_t irq, void *data)
{
	struct vcpu *vcpu;
	struct virq_desc *desc = (struct virq_desc *)data;

	if ((!desc) || (!desc->hw))
		return -EINVAL;

	/* send the virq to the guest */
	vcpu = get_vcpu_by_id(desc->vmid, desc->vcpu_id);
	if (!vcpu)
		return -ENOENT;

	return _send_virq(vcpu, desc->vno, irq, 1);
}

int register_virq(struct virqtag *v)
{
	struct virq_desc *desc;
	struct virq_domain *d;

	if (!v)
		return -EINVAL;

	d = get_virq_domain(v->vno);
	if (!d)
		return -ENOENT;

	desc = (struct virq_desc *)zalloc(sizeof(struct virq_desc));
	if (!desc)
		return -ENOMEM;

	desc->vno = v->vno;
	desc->hno = v->hno;
	desc->vmid = v->vmid;
	desc->vcpu_id = v->vcpu_id;
	desc->enable = 0;

	d->table[v->vno - d->start] = desc;

	if ((d->type == VIRQ_DOMAIN_SGI) || (d->type == VIRQ_DOMAIN_PPI)) {
		if (desc->hw) {
			pr_warning("virq:%d will not affinity to hirq\n", desc->vno);
			desc->hw = 0;
			desc->hno = 0;
		}
	}

	/*
	 * if the virq is point to a SPI int and the virq
	 * is affinity to a hw irq then request the irq
	 */
	if ((d->type == VIRQ_DOMAIN_SPI) && (desc->hw)) {
		request_irq(desc->hno, guest_irq_handler,
				0, v->name, (void *)desc);
		irq_mask(desc->hno);
	}

	return 0;
}

void virqs_init(void)
{
	alloc_virtual_irqs(VIRQ_BASE, MAX_VIRQ_NR,
			VIRQ_DOMAIN_VIRTUAL);

	register_hook(irq_exit_from_guest,
			MINOS_HOOK_TYPE_EXIT_FROM_GUEST);

	register_hook(irq_enter_to_guest,
			MINOS_HOOK_TYPE_ENTER_TO_GUEST);
}
