#include <mvisor/mvisor.h>
#include <asm/arch.h>
#include <mvisor/module.h>
#include <mvisor/mmio.h>
#include <mvisor/irq.h>
#include <asm/gicv3.h>
#include <mvisor/io.h>
#include <mvisor/module.h>
#include <mvisor/cpumask.h>

static struct list_head gicd_list;
static int vgic_module_id = INVAILD_MODULE_ID;

struct vgicv3_gicd {
	uint32_t gicd_ctlr;
	uint32_t gicd_typer;
	uint32_t vmid;
	struct list_head list;
	spinlock_t lock;
};

struct vgicv3_gicr {
	uint32_t gicr_ctlr;
	unsigned long rd_base;
	unsigned long sgi_base;
};

struct vgicv3 {
	struct vgicv3_gicd *gicd;
	struct vgicv3_gicr gicr;
};

static struct vgicv3_gicd *attach_vgicd(uint32_t vmid)
{
	struct vgicv3_gicd *gicd;

	list_for_each_entry(gicd, &gicd_list, list) {
		if (gicd->vmid == vmid)
			return gicd;
	}

	return NULL;
}

void vgic_send_sgi(struct vcpu_t *vcpu, unsigned long sgi_value)
{
	sgi_mode_t mode;
	uint32_t sgi;
	cpumask_t cpumask;
	unsigned long tmp, aff3, aff2, aff1;
	int bit, logic_cpu;
	vm_t *vm = vcpu->vm;

	sgi = (sgi_value & (0xf << 24)) >> 24;
	mode = sgi_value & (1 << 40) ? SGI_TO_OTHERS : SGI_TO_LIST;

	if (mode == SGI_TO_LIST) {
		tmp = sgi_value & 0xffff;
		aff3 = (sgi_value & (0xff << 48)) >> 48;
		aff2 = (sgi_value & (0xff << 32)) >> 32;
		aff1 = (sgi_value & (0xff << 16)) >> 16;
		for_each_set_bit(bit, &tmp, 16) {
			logic_cpu = affinity_to_logic_cpu(aff3, aff2, aff1, bit);
			cpumask_set_cpu(logic_cpu, &cpumask);
		}
	} else {
		for (bit = 0; bit < vm->vcpu_nr; bit++) {
			if (bit == vcpu->vcpu_id)
				continue;
			cpumask_set_cpu(bit, &cpumask);
		}
	}

	/*
	 * here we update the gicr releated register
	 * for some other purpose use TBD
	 */

	send_vsgi(vcpu, sgi, mode, &cpumask);
}

static void vgicv3_state_init(vcpu_t *vcpu, void *context)
{
	struct vgicv3 *vgic = (struct vgicv3 *)context;
	struct vgicv3_gicr *gicr = &vgic->gicr;
	unsigned long base;

	vgic->gicd = attach_vgicd(get_vmid(vcpu));
	if (vgic->gicd == NULL) {
		pr_error("can not find gicd for this vcpu\n");
		return;
	}


	/*
	 * int the gicr
	 */
	gicr->gicr_ctlr = 0;
	base = 0x2f100000 + (128 * 1024) * vcpu->vcpu_id;
	gicr->rd_base = base;
	gicr->sgi_base = base + (64 * 1024);
}

static void vgicv3_state_save(vcpu_t *vcpu, void *context)
{

}

static void vgicv3_state_restore(vcpu_t *vcpu, void *context)
{

}

static void vgicv3_vm_create(vm_t *vm)
{
	struct vgicv3_gicd *gicd;

	/*
	 * when a vm is created need to create
	 * one vgic for each vm since gicr is percpu
	 * but gicd is shared so created it here
	 */
	gicd = (struct vgicv3_gicd*)vmm_malloc(sizeof(struct vgicv3_gicd));
	if (!gicd)
		panic("No more memory for gicd\n");

	memset((char *)gicd, 0, sizeof(struct vgicv3_gicd));

	init_list(&gicd->list);
	gicd->vmid = vm->vmid;
	list_add_tail(&gicd_list, &gicd->list);
	spin_lock_init(&gicd->lock);

	if (vgic_module_id == INVAILD_MODULE_ID)
		vgic_module_id = get_module_id("vgic");

	/*
	 * init gicd TBD
	 */
	gicd->gicd_ctlr = 0;
	gicd->gicd_typer = ioread32((void *)0x2f000000 + GICD_TYPER);
}

#define GICV3_TYPE_GICD		(0x0)
#define GICV3_TYPE_GICR_RD	(0x1)
#define GICV3_TYPE_GICR_SGI	(0x2)
#define GICV3_TYPE_INVAILD	(0xff)

static int get_address_type(struct vgicv3 *vgic,
		unsigned long address, unsigned long *offset)
{
	if ((address >= 0x2f000000) && (address < 0x2f00ffff)) {
		*offset = address - 0x2f000000;
		return GICV3_TYPE_GICD;
	}

	if ((address >= vgic->gicr.rd_base) && (address < vgic->gicr.sgi_base)) {
		*offset = address - vgic->gicr.rd_base;
		return GICV3_TYPE_GICR_RD;
	}

	if ((address >= vgic->gicr.sgi_base) &&
		(address < (vgic->gicr.sgi_base + 64 * 1024))) {
		*offset = address - vgic->gicr.sgi_base;
		return GICV3_TYPE_GICR_SGI;
	}

	return GICV3_TYPE_INVAILD;
}

static int vgicv3_gicd_mmio_read(struct vgicv3 *vgic,
		unsigned long offset, unsigned long *value)
{
	struct vgicv3_gicd *gicd = vgic->gicd;

	spin_lock(&gicd->lock);

	switch (offset) {
	case GICD_CTLR:
		*value = gicd->gicd_ctlr & ~(1 << 31);
		break;
	case GICD_TYPER:
		*value = gicd->gicd_typer;
		break;
	case GICD_STATUSR:
		*value = 0;
		break;
	case GICD_ISENABLER...GICD_ISENABLER_END:
		*value = 0;
		break;
	case GICD_ICENABLER...GICD_ICENABLER_END:
		*value = 0;
		break;
	default:
		*value = 0;
		break;
	}

	spin_unlock(&gicd->lock);
	return 0;
}

static int vgicv3_gicr_rd_mmio_read(struct vgicv3 *vgic,
		unsigned long offset, unsigned long *value)
{
	return 0;
}

static int vgicv3_gicr_sgi_mmio_read(struct vgicv3 *vgic,
		unsigned long offset, unsigned long *value)
{
	struct vgicv3_gicr *gicr = &vgic->gicr;

	switch (offset) {
	case GICR_CTLR:
		*value = gicr->gicr_ctlr & ~(1 << 31);
		break;

	default:
		*value = 0;
		break;
	}

	return 0;
}

static int vgicv3_gicd_mmio_write(struct vgicv3 *vgic,
		unsigned long offset, unsigned long value)
{
	struct vgicv3_gicd *gicd = vgic->gicd;
	uint32_t x, y, bit;

	spin_lock(&gicd->lock);

	switch (offset) {
	case GICD_CTLR:
		gicd->gicd_ctlr = value;
		break;
	case GICD_TYPER:
		break;
	case GICD_STATUSR:
		break;
	case GICD_ISENABLER...GICD_ISENABLER_END:
		x = (offset - GICD_ISENABLER) / 4;
		y = x * 32;
		for_each_set_bit(bit, &value, 32)
			virq_unmask(y + bit);
		break;
	case GICD_ICENABLER...GICD_ICENABLER_END:
		x = (offset - GICD_ICENABLER) / 4;
		y = x * 32;
		for_each_set_bit(bit, &value, 32)
			virq_mask(y + bit);
		break;
	}

	spin_unlock(&gicd->lock);
	return 0;
}

static int vgicv3_gicr_rd_mmio_write(struct vgicv3 *vgic,
		unsigned long offset, unsigned long value)
{
	return 0;
}

static int vgicv3_gicr_sgi_mmio_write(struct vgicv3 *vgic,
		unsigned long offset, unsigned long value)
{
	return 0;
}

static int vgicv3_mmio_read(vcpu_regs *regs,
		unsigned long address,unsigned long *read_value)
{
	int type;
	unsigned long offset;
	struct vgicv3 *vgic = (struct vgicv3 *)
			get_module_data_by_id((vcpu_t *)regs, vgic_module_id);

	type = get_address_type(vgic, address, &offset);
	switch (type) {
	case GICV3_TYPE_GICD:
		return vgicv3_gicd_mmio_read(vgic, offset, read_value);

	case GICV3_TYPE_GICR_RD:
		return vgicv3_gicr_rd_mmio_read(vgic, offset, read_value);

	case GICV3_TYPE_GICR_SGI:
		return vgicv3_gicr_sgi_mmio_read(vgic, offset, read_value);

	default:
		pr_error("invaild gic address 0x%x\n", address);
		return -EINVAL;
	}

	return -EINVAL;
}

static int vgicv3_mmio_write(vcpu_regs *regs,
		unsigned long address, unsigned long write_value)
{
	int type;
	unsigned long offset;
	struct vgicv3 *vgic = (struct vgicv3 *)
			get_module_data_by_id((vcpu_t *)regs, vgic_module_id);

	type = get_address_type(vgic, address, &offset);
	switch (type) {
	case GICV3_TYPE_GICD:
		return vgicv3_gicd_mmio_write(vgic, offset, write_value);

	case GICV3_TYPE_GICR_RD:
		return vgicv3_gicr_rd_mmio_write(vgic, offset, write_value);

	case GICV3_TYPE_GICR_SGI:
		return vgicv3_gicr_sgi_mmio_write(vgic, offset, write_value);

	default:
		pr_error("invaild gic address 0x%x\n", address);
		return -EINVAL;
	}

	return -EINVAL;
}

static int vgicv3_mmio_check(vcpu_regs *regs, unsigned long address)
{
	if ((address >= 0x2f000000) && (address < 0x2f2000000))
		return 1;

	return 0;
}

static struct mmio_ops vgicv3_mmio_ops = {
	.read = vgicv3_mmio_read,
	.write = vgicv3_mmio_write,
	.check = vgicv3_mmio_check,
};

static int vgicv3_module_init(struct vmm_module *module)
{
	init_list(&gicd_list);

	module->context_size = sizeof(struct vgicv3);
	module->pdata = NULL;
	module->state_init = vgicv3_state_init;
	module->state_save = vgicv3_state_save;
	module->state_restore = vgicv3_state_restore;
	module->create_vm = vgicv3_vm_create;

	register_mmio_emulation_handler("vgicv3", &vgicv3_mmio_ops);

	return 0;
}

VMM_MODULE_DECLARE(vgic_v3, "vgic-v3",
		"vgic", (void *)vgicv3_module_init);
