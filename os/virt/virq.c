/*
 * Copyright (C) 2018 Min Le (lemin9538@gmail.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <minos/minos.h>
#include <minos/irq.h>
#include <minos/sched.h>
#include <virt/virq.h>
#include <virt/virq_chip.h>

static DEFINE_SPIN_LOCK(hvm_irq_lock);

static inline struct virq_desc *
get_virq_desc(struct vcpu *vcpu, uint32_t virq)
{
	struct vm *vm = vcpu->vm;

	if (virq < VM_LOCAL_VIRQ_NR)
		return &vcpu->virq_struct->local_desc[virq];

	if (virq >= VM_VIRQ_NR(vm->vspi_nr))
		return NULL;

	return &vm->vspi_desc[VIRQ_SPI_OFFSET(virq)];
}

static void inline virq_kick_vcpu(struct vcpu *vcpu,
		struct virq_desc *desc)
{
	kick_vcpu(vcpu, virq_is_hw(desc));
}

static int inline __send_virq(struct vcpu *vcpu, struct virq_desc *desc)
{
	unsigned long flags;
	struct virq_struct *virq_struct = vcpu->virq_struct;

	spin_lock_irqsave(&virq_struct->lock, flags);

	/*
	 * if the virq is already at the pending state, do
	 * nothing, other case need to send it to the vcpu
	 * if the virq is in offline state, send it to vcpu
	 * directly
	 */
	if (virq_is_pending(desc)) {
		spin_unlock_irqrestore(&virq_struct->lock, flags);
		return 0;
	}

	virq_set_pending(desc);
	dsb();

	/*
	 * if desc->list.next is not NULL, the virq is in
	 * actvie or pending list do not change it
	 */
	if (desc->list.next == NULL) {
		list_add_tail(&virq_struct->pending_list, &desc->list);
		virq_struct->active_count++;
	}

	if (desc->vno < VM_SGI_VIRQ_NR)
		desc->src = get_vcpu_id(get_current_vcpu());

	spin_unlock_irqrestore(&virq_struct->lock, flags);
	return 0;
}

static int send_virq(struct vcpu *vcpu, struct virq_desc *desc)
{
	int ret;
	struct vm *vm = vcpu->vm;

	/* do not send irq to vm if not online or suspend state */
	if ((vm->state == VM_STAT_OFFLINE) ||
			(vm->state == VM_STAT_REBOOT)) {
		pr_warn("send virq failed vm is offline or reboot\n");
		return -EINVAL;
	}

	/*
	 * check the state of the vm, if the vm
	 * is in suspend state and the irq can not
	 * wake up the vm, just return other wise
	 * need to kick the vcpu
	 */
	if (vm->state == VM_STAT_SUSPEND) {
		if (!virq_can_wakeup(desc)) {
			pr_warn("send virq failed vm is suspend\n");
			return -EAGAIN;
		}
	}

	ret = __send_virq(vcpu, desc);
	if (ret) {
		pr_warn("send virq to vcpu-%d-%d failed\n",
				get_vmid(vcpu), get_vcpu_id(vcpu));
		return ret;
	}

	virq_kick_vcpu(vcpu, desc);

	return 0;
}

static int guest_irq_handler(uint32_t irq, void *data)
{
	struct vcpu *vcpu;
	struct virq_desc *desc = (struct virq_desc *)data;

	if ((!desc) || (!virq_is_hw(desc))) {
		pr_info("virq %d is not a hw irq\n", desc->vno);
		return -EINVAL;
	}

	/* send the virq to the guest */
	if ((desc->vmid == VIRQ_AFFINITY_VM_ANY) &&
			(desc->vcpu_id == VIRQ_AFFINITY_VCPU_ANY))
		vcpu = get_current_vcpu();
	else
		vcpu = get_vcpu_by_id(desc->vmid, desc->vcpu_id);

	if (!vcpu) {
		pr_err("%s: Can not get the vcpu for irq:%d\n", irq);
		return -ENOENT;
	}

	return send_virq(vcpu, desc);
}

uint32_t virq_get_type(struct vcpu *vcpu, uint32_t virq)
{
	struct virq_desc *desc;

	desc = get_virq_desc(vcpu, virq);
	if (!desc)
		return 0;

	return desc->type;
}

uint32_t virq_get_state(struct vcpu *vcpu, uint32_t virq)
{
	struct virq_desc *desc;

	desc = get_virq_desc(vcpu, virq);
	if (!desc)
		return 0;

	return !!virq_is_enabled(desc);
}

uint32_t virq_get_affinity(struct vcpu *vcpu, uint32_t virq)
{
	struct virq_desc *desc;

	desc = get_virq_desc(vcpu, virq);
	if (!desc)
		return 0;

	return desc->vcpu_id;
}

uint32_t virq_get_pr(struct vcpu *vcpu, uint32_t virq)
{
	struct virq_desc *desc;

	desc = get_virq_desc(vcpu, virq);
	if (!desc)
		return 0;

	return desc->pr;
}

int virq_set_type(struct vcpu *vcpu, uint32_t virq, int value)
{
	struct virq_desc *desc;

	desc = get_virq_desc(vcpu, virq);
	if (!desc)
		return -ENOENT;

	/*
	 * 0 - IRQ_TYPE_LEVEL_HIGH
	 * 1 - IRQ_TYPE_EDGE_RISING
	 */
	if (desc->type != value) {
		desc->type = value;
		if (virq_is_hw(desc)) {
			if (value)
				value = IRQ_FLAGS_EDGE_RISING;
			else
				value = IRQ_FLAGS_LEVEL_HIGH;

			irq_set_type(desc->hno, value);
		}
	}

	return 0;
}

int virq_set_priority(struct vcpu *vcpu, uint32_t virq, int pr)
{
	struct virq_desc *desc;

	desc = get_virq_desc(vcpu, virq);
	if (!desc) {
		pr_debug("virq is no exist %d\n", virq);
		return -ENOENT;
	}

	pr_debug("set the pr:%d for virq:%d\n", pr, virq);
	desc->pr = pr;

	return 0;
}

int virq_enable(struct vcpu *vcpu, uint32_t virq)
{
	struct virq_desc *desc;

	desc = get_virq_desc(vcpu, virq);
	if (!desc)
		return -ENOENT;

	virq_set_enable(desc);

	if (virq > VM_LOCAL_VIRQ_NR) {
		if (virq_is_hw(desc))
			irq_unmask(desc->hno);
	}

	return 0;
}

int virq_disable(struct vcpu *vcpu, uint32_t virq)
{
	struct virq_desc *desc;

	desc = get_virq_desc(vcpu, virq);
	if (!desc)
		return -ENOENT;

	virq_clear_enable(desc);

	if (virq > VM_LOCAL_VIRQ_NR) {
		if (virq_is_hw(desc))
			irq_mask(desc->hno);
	}

	return 0;
}

int send_virq_to_vcpu(struct vcpu *vcpu, uint32_t virq)
{
	struct virq_desc *desc;

	desc = get_virq_desc(vcpu, virq);

	return send_virq(vcpu, desc);
}

int send_virq_to_vm(struct vm *vm, uint32_t virq)
{
	struct virq_desc *desc;
	struct vcpu *vcpu;

	if ((!vm) || (virq < VM_LOCAL_VIRQ_NR))
		return -EINVAL;

	desc = get_virq_desc(vm->vcpus[0], virq);
	if (!desc)
		return -ENOENT;

	if (virq_is_hw(desc)) {
		pr_err("can not send hw irq in here\n");
		return -EPERM;
	}

	vcpu = get_vcpu_in_vm(vm, desc->vcpu_id);
	if (!vcpu)
		return -ENOENT;

	return send_virq(vcpu, desc);
}

void send_vsgi(struct vcpu *sender, uint32_t sgi, cpumask_t *cpumask)
{
	int cpu;
	struct vcpu *vcpu;
	struct vm *vm = sender->vm;
	struct virq_desc *desc;

	for_each_set_bit(cpu, cpumask->bits, vm->vcpu_nr) {
		vcpu = vm->vcpus[cpu];
		desc = get_virq_desc(vcpu, sgi);
		send_virq(vcpu, desc);
	}
}

void clear_pending_virq(struct vcpu *vcpu, uint32_t irq)
{
	unsigned long flags;
	struct virq_desc *desc;
	struct virq_struct *virq_struct = vcpu->virq_struct;

	desc = get_virq_desc(vcpu, irq);
	if ((!desc) || (desc->state != VIRQ_STATE_ACTIVE))
		return;

	/*
	 * this function can only called by the current
	 * running vcpu and excuted on the related pcpu
	 *
	 * check wether the virq is pending agagin, if yes
	 * do not delete it from the pending list, instead
	 * of add it to the tail of the pending list
	 *
	 */
	spin_lock_irqsave(&virq_struct->lock, flags);
	if (desc->list.next != NULL) {
		list_del(&desc->list);
		desc->list.next = NULL;
	}

	if (virq_is_pending(desc)) {
		list_add_tail(&virq_struct->pending_list, &desc->list);
		desc->state = VIRQ_STATE_PENDING;
		goto out;
	}

	desc->state = VIRQ_STATE_INACTIVE;
out:
	virqchip_update_virq(vcpu, desc, VIRQ_ACTION_CLEAR);
	spin_unlock_irqrestore(&virq_struct->lock, flags);
}

uint32_t get_pending_virq(struct vcpu *vcpu)
{
	unsigned long flags;
	struct virq_desc *desc;
	struct virq_struct *virq_struct = vcpu->virq_struct;

	spin_lock_irqsave(&virq_struct->lock, flags);
	if (is_list_empty(&virq_struct->pending_list)) {
		spin_unlock_irqrestore(&virq_struct->lock, flags);
		return BAD_IRQ;
	}

	/* get the pending virq and delete it from pending list */
	desc = list_first_entry(&virq_struct->pending_list,
			struct virq_desc, list);
	list_del(&desc->list);
	list_add_tail(&virq_struct->active_list, &desc->list);
	desc->state = VIRQ_STATE_ACTIVE;
	virq_clear_pending(desc);

	spin_unlock_irqrestore(&virq_struct->lock, flags);

	return desc->vno;
}

int vcpu_has_irq(struct vcpu *vcpu)
{
	struct virq_struct *vs = vcpu->virq_struct;
	int pend, active;

	pend = is_list_empty(&vs->pending_list);
	active = is_list_empty(&vs->active_list);

	return !(pend && active);
}

void vcpu_virq_struct_reset(struct vcpu *vcpu)
{
	int i;
	struct virq_desc *desc;
	struct virq_struct *virq_struct = vcpu->virq_struct;

	virq_struct->active_count = 0;
	spin_lock_init(&virq_struct->lock);
	init_list(&virq_struct->pending_list);
	init_list(&virq_struct->active_list);
	virq_struct->pending_virq = 0;
	virq_struct->pending_hirq = 0;
	memset(virq_struct->irq_bitmap, 0, sizeof(virq_struct->irq_bitmap));

	for (i = 0; i < VM_LOCAL_VIRQ_NR; i++) {
		desc = &virq_struct->local_desc[i];
		desc->id = VIRQ_INVALID_ID;
		desc->list.next = NULL;
		desc->state = VIRQ_STATE_INACTIVE;
		virq_clear_pending(desc);
	}
}

static void update_virq_cap(struct virq_desc *desc, unsigned long flags)
{
	if (flags & VIRQF_CAN_WAKEUP)
		virq_set_wakeup(desc);

	if (flags & VIRQF_ENABLE) {
		virq_set_enable(desc);
		if (virq_is_hw(desc))
			irq_unmask(desc->hno);
	}
}

void vcpu_virq_struct_init(struct vcpu *vcpu)
{
	int i;
	struct virq_desc *desc;
	struct virq_struct *virq_struct = vcpu->virq_struct;

	if (!virq_struct)
		return;

	virq_struct->active_count = 0;
	spin_lock_init(&virq_struct->lock);
	init_list(&virq_struct->pending_list);
	init_list(&virq_struct->active_list);
	virq_struct->pending_virq = 0;
	virq_struct->pending_hirq = 0;

	memset(&virq_struct->local_desc, 0,
		sizeof(struct virq_desc) * VM_LOCAL_VIRQ_NR);

	for (i = 0; i < VM_LOCAL_VIRQ_NR; i++) {
		desc = &virq_struct->local_desc[i];
		virq_clear_hw(desc);
		virq_set_enable(desc);

		/* this is just for ppi or sgi */
		desc->vcpu_id = VIRQ_AFFINITY_VCPU_ANY;
		desc->vmid = VIRQ_AFFINITY_VM_ANY;
		desc->vno = i;
		desc->hno = 0;
		desc->id = VIRQ_INVALID_ID;
		desc->list.next = NULL;
		desc->state = VIRQ_STATE_INACTIVE;
	}
}

static int __request_virq(struct vcpu *vcpu, struct virq_desc *desc,
			uint32_t virq, uint32_t hwirq, unsigned long flags)
{
	if (desc->vno && (desc->vno != virq))
		pr_warn("virq-%d may has been requested\n", virq);

	pr_debug("vm-%d request virq %d --> hwirq %d\n", virq, hwirq);

	desc->vno = virq;
	desc->hno = hwirq;
	desc->vcpu_id = get_vcpu_id(vcpu);
	desc->pr = 0xa0;
	desc->vmid = get_vmid(vcpu);
	desc->id = VIRQ_INVALID_ID;
	desc->list.next = NULL;
	desc->state = VIRQ_STATE_INACTIVE;
	virq_clear_enable(desc);
	if (virq >= VM_LOCAL_VIRQ_NR)
		set_bit(VIRQ_SPI_OFFSET(virq), vcpu->vm->vspi_map);

	/* if the virq affinity to a hwirq need to request
	 * the hw irq */
	if (hwirq) {
		irq_set_affinity(hwirq, vcpu_affinity(vcpu));
		virq_set_hw(desc);
		request_irq(hwirq, guest_irq_handler, IRQ_FLAGS_VCPU,
				vcpu->task->name, (void *)desc);
		irq_mask(desc->hno);
	} else
		virq_clear_hw(desc);

	update_virq_cap(desc, flags);

	return 0;
}

int request_virq_affinity(struct vm *vm, uint32_t virq, uint32_t hwirq,
			int affinity, unsigned long flags)
{
	struct vcpu *vcpu;
	struct virq_desc *desc;

	if (!vm)
		return -EINVAL;

	vcpu = get_vcpu_in_vm(vm, affinity);
	if (!vcpu) {
		pr_err("request virq fail no vcpu-%d in vm-%d\n",
				affinity, vm->vmid);
		return -EINVAL;
	}

	desc = get_virq_desc(vcpu, virq);
	if (!desc) {
		pr_err("virq-%d not exist vm-%d", virq, vm->vmid);
		return -ENOENT;
	}

	return __request_virq(vcpu, desc, virq, hwirq, flags);
}

int request_hw_virq(struct vm *vm, uint32_t virq, uint32_t hwirq,
			unsigned long flags)
{
	int max;

	if (vm_is_hvm(vm))
		max = MAX_HVM_VIRQ;
	else
		max = MAX_GVM_VIRQ;
	if (virq >= max) {
		pr_err("invaild virq-%d for vm-%d\n", virq, vm->vmid);
		return -EINVAL;
	}

	return request_virq_affinity(vm, virq, hwirq, 0, flags);
}

int request_virq(struct vm *vm, uint32_t virq, unsigned long flags)
{
	return request_hw_virq(vm, virq, 0, flags);
}

int request_virq_pervcpu(struct vm *vm, uint32_t virq, unsigned long flags)
{
	int ret;
	struct vcpu *vcpu;
	struct virq_desc *desc;

	if (virq >= VM_LOCAL_VIRQ_NR)
		return -EINVAL;

	vm_for_each_vcpu(vm, vcpu) {
		desc = get_virq_desc(vcpu, virq);
		if (!desc)
			continue;

		ret = __request_virq(vcpu, desc, virq, 0, flags);
		if (ret) {
			pr_err("request percpu virq-%d failed vm-%d\n",
					virq, vm->vmid);
		}

		/*
		 * Fix me here may need to update the affinity for
		 * thie virq if it is a ppi or sgi, if the ppi is
		 * bind to the hw ppi, need to do this, otherwise
		 * do not need to change it
		 */
		desc->vcpu_id = VIRQ_AFFINITY_VCPU_ANY;
		desc->vmid = VIRQ_AFFINITY_VM_ANY;
	}

	return 0;
}

int alloc_vm_virq(struct vm *vm)
{
	int virq;
	int count = vm->vspi_nr;

	if (vm_is_hvm(vm))
		spin_lock(&hvm_irq_lock);

	virq = find_next_zero_bit_loop(vm->vspi_map, count, 0);
	if (virq >= count)
		virq = -1;

	if (virq >= 0)
		request_virq(vm, virq + VM_LOCAL_VIRQ_NR, VIRQF_ENABLE);

	if (vm_is_hvm(vm))
		spin_unlock(&hvm_irq_lock);

	return (virq >= 0 ? virq + VM_LOCAL_VIRQ_NR : -1);
}

void release_vm_virq(struct vm *vm, int virq)
{
	struct virq_desc *desc;

	virq = VIRQ_SPI_OFFSET(virq);
	if (virq >= vm->vspi_nr)
		return;

	if (vm_is_hvm(vm))
		spin_lock(&hvm_irq_lock);

	desc = &vm->vspi_desc[virq];
	memset(desc, 0, sizeof(struct virq_desc));
	clear_bit(virq, vm->vspi_map);

	if (vm_is_hvm(vm))
		spin_unlock(&hvm_irq_lock);
}

static int virq_create_vm(void *item, void *args)
{
	uint32_t vspi_nr;
	uint32_t size, tmp, map_size;
	struct vm *vm = (struct vm *)item;

	if (vm->vmid == 0)
		vspi_nr = HVM_SPI_VIRQ_NR;
	else
		vspi_nr = GVM_SPI_VIRQ_NR;

	tmp = sizeof(struct virq_desc) * vspi_nr;
	tmp = BALIGN(tmp, sizeof(unsigned long));
	size = PAGE_BALIGN(tmp);
	vm->virq_same_page = 0;
	vm->vspi_desc = get_free_pages(PAGE_NR(size));
	if (!vm->vspi_desc)
		return -ENOMEM;

	map_size = BITS_TO_LONGS(vspi_nr) * sizeof(unsigned long);

	if ((size - tmp) >= map_size) {
		vm->vspi_map = (unsigned long *)
			((unsigned long)vm->vspi_desc + tmp);
		vm->virq_same_page = 1;
	} else {
		vm->vspi_map = malloc(BITS_TO_LONGS(vspi_nr) *
				sizeof(unsigned long));
		if (!vm->vspi_map)
			return -ENOMEM;
	}

	memset(vm->vspi_desc, 0, tmp);
	memset(vm->vspi_map, 0, BITS_TO_LONGS(vspi_nr) *
			sizeof(unsigned long));
	vm->vspi_nr = vspi_nr;

	return 0;
}

void vm_virq_reset(struct vm *vm)
{
	int i;
	struct virq_desc *desc;

	/* reset the all the spi virq for the vm */
	for ( i = 0; i < vm->vspi_nr; i++) {
		desc = &vm->vspi_desc[i];
		virq_clear_enable(desc);
		virq_clear_pending(desc);
		desc->pr = 0xa0;
		desc->type = 0x0;
		desc->id = VIRQ_INVALID_ID;
		desc->state = VIRQ_STATE_INACTIVE;
		desc->list.next = NULL;

		if (virq_is_hw(desc))
			irq_mask(desc->hno);
	}
}

static int virq_destroy_vm(void *item, void *data)
{
	int i;
	struct virq_desc *desc;
	struct vm *vm = (struct vm *)item;

	if (vm->vspi_desc) {
		for (i = 0; i < VIRQ_SPI_NR(vm->vspi_nr); i++) {
			desc = &vm->vspi_desc[i];

			/* should check whether the hirq is pending or not */
			if (virq_is_enabled(desc) && virq_is_hw(desc) &&
					desc->hno > VM_LOCAL_VIRQ_NR)
				irq_mask(desc->hno);
		}

		free(vm->vspi_desc);
	}

	if (!vm->virq_same_page)
		free(vm->vspi_map);

	return 0;
}

void virqs_init(void)
{
	register_hook(virq_create_vm, OS_HOOK_CREATE_VM);
	register_hook(virq_destroy_vm, OS_HOOK_DESTROY_VM);
}
