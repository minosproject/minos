#ifndef __MINOS_HYPERCALL_H__
#define __MINOS_HYPERCALL_h__

/* below defination is for HVC call */
#define HVC_TYPE_HVC_VCPU		(0x7)
#define HVC_TYPE_HVC_VM			(0x8)
#define HVC_TYPE_HVC_PM			(0x9)
#define HVC_TYPE_HVC_MISC		(0xa)
#define HVC_TYPE_HVC_VM0		(0xa)

#define HVC_CALL_BASE			(0xc0000000)

#define HVC_VCPU_FN(n)			(HVC_CALL_BASE + (HVC_TYPE_HVC_VCPU << 24) + n)
#define HVC_VM_FN(n)			(HVC_CALL_BASE + (HVC_TYPE_HVC_VM << 24) + n)
#define HVC_PM_FN(n) 			(HVC_CALL_BASE + (HVC_TYPE_HVC_PM << 24) + n)
#define HVC_MISC_FN(n)			(HVC_CALL_BASE + (HVC_TYPE_HVC_MISC << 24) + n)
#define HVC_VM0_FN(n)			(HVC_CALL_BASE + (HVC_TYPE_HVC_VM0 << 24) + n)

#define	HVC_VM_CREATE			HVC_VM_FN(0)
#define HVC_VM_DESTORY			HVC_VM_FN(1)
#define HVC_VM_RESTART			HVC_VM_FN(2)
#define HVC_VM_POWER_UP			HVC_VM_FN(3)
#define HVC_VM_POWER_DOWN		HVC_VM_FN(4)

#endif
