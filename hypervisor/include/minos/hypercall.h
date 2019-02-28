#ifndef __MINOS_HYPERCALL_H__
#define __MINOS_HYPERCALL_h__

/* below defination is for HVC call */
#define HVC_TYPE_HVC_VM0		(0x8)
#define HVC_TYPE_HVC_MISC		(0x9)

#define HVC_CALL_BASE			(0xc0000000)

#define HVC_VM0_FN(n)			(HVC_CALL_BASE + (HVC_TYPE_HVC_VM0 << 24) + n)
#define HVC_MISC_FN(n)			(HVC_CALL_BASE + (HVC_TYPE_HVC_MISC << 24) + n)

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
#define HVC_VM_CREATE_HOST_VDEV		HVC_VM0_FN(13)

#endif
