#ifndef __MINOS_HYPERCALL_H__
#define __MINOS_HYPERCALL_h__

/* below defination is for HVC call */
#define HVC_TYPE_HVC_VCPU		(0x7)
#define HVC_TYPE_HVC_VM			(0x8)
#define HVC_TYPE_HVC_PM			(0x9)
#define HVC_TYPE_HVC_MISC		(0xa)
#define HVC_TYPE_HVC_VIRTIO		(0xb)

#define HVC_CALL_BASE			(0xc0000000)

#define HVC_VCPU_FN(n)			(HVC_CALL_BASE + (HVC_TYPE_HVC_VCPU << 24) + n)
#define HVC_VM_FN(n)			(HVC_CALL_BASE + (HVC_TYPE_HVC_VM << 24) + n)
#define HVC_PM_FN(n) 			(HVC_CALL_BASE + (HVC_TYPE_HVC_PM << 24) + n)
#define HVC_MISC_FN(n)			(HVC_CALL_BASE + (HVC_TYPE_HVC_MISC << 24) + n)
#define HVC_VIRTIO_FN(n)		(HVC_CALL_BASE + (HVC_TYPE_HVC_VIRTIO << 24) + n)

/* hypercall for vm releated operation */
#define	HVC_VM_CREATE			HVC_VM_FN(0)
#define HVC_VM_DESTORY			HVC_VM_FN(1)
#define HVC_VM_RESTART			HVC_VM_FN(2)
#define HVC_VM_POWER_UP			HVC_VM_FN(3)
#define HVC_VM_POWER_DOWN		HVC_VM_FN(4)
#define HVC_VM_MMAP			HVC_VM_FN(5)
#define HVC_VM_UNMMAP			HVC_VM_FN(6)
#define HVC_VM_SEND_VIRQ		HVC_VM_FN(7)

/* hypercall for virtio releate operation */
#define HVC_VIRTIO_CREATE_DEVICE	HVC_VIRTIO_FN(0)

#endif
