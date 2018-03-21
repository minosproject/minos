#include <mvisor/mvisor.h>
#include <asm/arch.h>
#include <mvisor/module.h>

static struct gicd_list;

struct vgicv3_gicd {
	uint32_t vmid;
	struct list_head list;
};

struct vgicv3_gicr {

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

static void vgicv3_state_init(vcpu_t *vcpu, void *context)
{
	struct vgicv3 *vgic = (struct vgicv3 *)context;
	struct vgicv3_gicr *gicr->gicr;

	vgic->gicd = attach_vgicd(get_vmid(vpud));
	if (vgic->gicd == NULL) {
		pr_error("can not find gicd for this vcpu\n");
		return;
	}

	/*
	 * int the gicr
	 */
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

	/*
	 * init gicd TBD
	 */
}

static int vgicv3_module_init(struct vmm_module *module)
{
	init_list(&gicd_list);

	module->context_size = sizeof(struct vgicv3);
	module->pdata = NULL;
	module->state_init = vgicv3_state_init;
	module->state_save = vgicv3_state_save;
	module->state_restore = vgicv3_state_restore;
	module->create_vm = vgicv3_vm_create;

	return 0;
}

VMM_MODULE_DECLARE(vgic_v3, "vgic-v3",
		"vgic", (void *)vgicv3_module_init);
