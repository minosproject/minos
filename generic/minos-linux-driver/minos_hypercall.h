#ifndef __MINOS_HYPERCALL_H__
#define __MINOS_HYPERCALL_H__

/* below defination is for HVC call */
#define HVC_TYPE_VM0			(0x8)
#define HVC_TYPE_MISC			(0x9)
#define HVC_TYPE_VMBOX			(0xa)
#define HVC_TYPE_DEBUG_CONSOLE		(0xb)

#define HVC_CALL_BASE			(0xc0000000)

#define HVC_CALL_NUM(t, n)		(HVC_CALL_BASE + (t << 24) + n)

#define HVC_VM0_FN(n)			HVC_CALL_NUM(HVC_TYPE_VM0, n)
#define HVC_MISC_FN(n)			HVC_CALL_NUM(HVC_TYPE_MISC, n)
#define HVC_VMBOX_FN(n)			HVC_CALL_NUM(HVC_TYPE_VMBOX, n)
#define HVC_VMBOX_DEBUG_CONSOLE(n)	HVC_CALL_NUM(HVC_TYPE_DEBUG_CONSOLE, n)

/* hypercall for vm releated operation */
#define	HVC_VM_CREATE			HVC_VM0_FN(0)
#define HVC_VM_DESTORY			HVC_VM0_FN(1)
#define HVC_VM_RESTART			HVC_VM0_FN(2)
#define HVC_VM_POWER_UP			HVC_VM0_FN(3)
#define HVC_VM_POWER_DOWN		HVC_VM0_FN(4)
#define HVC_VM_MMAP			HVC_VM0_FN(5)
#define HVC_VM_UNMMAP			HVC_VM0_FN(6)
#define HVC_VM_SEND_VIRQ		HVC_VM0_FN(7)
#define HVC_VM_CREATE_VMCS		HVC_VM0_FN(8)
#define HVC_VM_CREATE_VMCS_IRQ		HVC_VM0_FN(9)
#define HVC_VM_REQUEST_VIRQ		HVC_VM0_FN(10)
#define HVC_VM_VIRTIO_MMIO_INIT		HVC_VM0_FN(11)
#define HVC_VM_VIRTIO_MMIO_DEINIT	HVC_VM0_FN(12)
#define HVC_VM_CREATE_RESOURCE		HVC_VM0_FN(13)
#define HVC_CHANGE_LOG_LEVEL		HVC_VM0_FN(14)

#define HVC_GET_VMID			HVC_MISC_FN(0)
#define HVC_SCHED_OUT			HVC_MISC_FN(1)

#define HVC_GET_VM_CAP			HVC_MISC_FN(2)
#define VM_CAP_HOST			(1 << 0)
#define VM_CAP_NATIVE			(1 << 1)

#define HVC_DC_GET_STAT			HVC_VMBOX_DEBUG_CONSOLE(0)
#define HVC_DC_GET_RING			HVC_VMBOX_DEBUG_CONSOLE(1)
#define HVC_DC_GET_IRQ			HVC_VMBOX_DEBUG_CONSOLE(2)
#define HVC_DC_WRITE			HVC_VMBOX_DEBUG_CONSOLE(3)
#define HVC_DC_OPEN			HVC_VMBOX_DEBUG_CONSOLE(4)
#define HVC_DC_CLOSE			HVC_VMBOX_DEBUG_CONSOLE(5)

static inline unsigned long __minos_hvc(uint32_t id, unsigned long a0,
       unsigned long a1, unsigned long a2, unsigned long a3,
       unsigned long a4, unsigned long a5)
{
   struct arm_smccc_res res;

   arm_smccc_hvc(id, a0, a1, a2, a3, a4, a5, 0, &res);
   return res.a0;
}

#define minos_hvc(id, a, b, c, d, e, f) \
	__minos_hvc(id, (unsigned long)(a), (unsigned long)(b), \
		    (unsigned long)(c), (unsigned long)(d), \
		    (unsigned long)(e), (unsigned long)(f))

#define minos_hvc0(id) 				minos_hvc(id, 0, 0, 0, 0, 0, 0)
#define minos_hvc1(id, a)			minos_hvc(id, a, 0, 0, 0, 0, 0)
#define minos_hvc2(id, a, b)			minos_hvc(id, a, b, 0, 0, 0, 0)
#define minos_hvc3(id, a, b, c) 		minos_hvc(id, a, b, c, 0, 0, 0)
#define minos_hvc4(id, a, b, c, d)		minos_hvc(id, a, b, c, d, 0, 0)
#define minos_hvc5(id, a, b, c, d, e)		minos_hvc(id, a, b, c, d, e, 0)
#define minos_hvc6(id, a, b, c, d, e, f)	minos_hvc(id, a, b, c, d, e, f)

static inline int hvc_vm_create(void *vmtag)
{
	return minos_hvc1(HVC_VM_CREATE, vmtag);
}

static inline int hvc_vm_destroy(int vmid)
{
	return minos_hvc1(HVC_VM_DESTORY, vmid);
}

static inline int hvc_vm_reset(int vmid)
{
	return minos_hvc1(HVC_VM_RESTART, vmid);
}

static inline int hvc_vm_power_up(int vmid)
{
	return minos_hvc1(HVC_VM_POWER_UP, vmid);
}

static inline int hvc_vm_mmap(int vmid, unsigned long offset,
		unsigned long size, unsigned long *addr)
{
	struct arm_smccc_res res;

	arm_smccc_hvc(HVC_VM_MMAP, vmid, offset, size, 0, 0, 0, 0, &res);

	*addr = res.a1;

	return (int)res.a0;
}

static inline int hvc_vm_unmap(int vmid)
{
	return minos_hvc1(HVC_VM_UNMMAP, vmid);
}

static inline int hvc_send_virq(int vmid, uint32_t virq)
{
	return minos_hvc2(HVC_VM_SEND_VIRQ, vmid, virq);
}

static inline unsigned long hvc_create_vmcs(int vmid)
{
	return minos_hvc1(HVC_VM_CREATE_VMCS, vmid);
}

static inline int hvc_create_vmcs_irq(int vmid, int vcpu_id)
{
	return minos_hvc2(HVC_VM_CREATE_VMCS_IRQ, vmid, vcpu_id);
}

static inline int hvc_virtio_mmio_deinit(int vmid)
{
	return minos_hvc1(HVC_VM_VIRTIO_MMIO_DEINIT, vmid);
}

static inline int hvc_virtio_mmio_init(int vmid, unsigned long gbase,
		size_t size, unsigned long *hbase)
{
	struct arm_smccc_res res;

	arm_smccc_hvc(HVC_VM_VIRTIO_MMIO_INIT,
			vmid, gbase, size, 0, 0, 0, 0, &res);
	*hbase = res.a1;

	return (int)res.a0;
}

static inline int hvc_create_vm_resource(int vmid)
{
	return minos_hvc1(HVC_VM_CREATE_RESOURCE, vmid);
}

static inline int hvc_request_virq(int vmid, int base, int nr)
{
	return minos_hvc3(HVC_VM_REQUEST_VIRQ, vmid, base, nr);
}

static inline int hvc_change_log_level(unsigned int level)
{
	return minos_hvc1(HVC_CHANGE_LOG_LEVEL, level);
}

static inline int hvc_sched_out(void)
{
	return minos_hvc0(HVC_SCHED_OUT);
}

static inline int get_vmid(void)
{
	return minos_hvc0(HVC_GET_VMID);
}

static inline unsigned long get_vm_capability(void)
{
	return minos_hvc0(HVC_GET_VM_CAP);
}

static inline int vm_is_host_vm(void)
{
	return !!(get_vm_capability() & VM_CAP_HOST);
}

#endif
