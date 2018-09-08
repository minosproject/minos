#ifndef __MINOS_IOCTL_H__
#define __MINOS_IOCTL_H__

#define IOCTL_CREATE_VM			(0xf000)
#define IOCTL_DESTROY_VM		(0xf001)
#define IOCTL_RESTART_VM		(0xf002)
#define IOCTL_POWER_DOWN_VM		(0xf003)
#define IOCTL_POWER_UP_VM		(0xf004)
#define IOCTL_VM_MMAP			(0xf005)
#define IOCTL_VM_UNMAP			(0xf006)
#define IOCTL_REGISTER_VCPU		(0xf007)
#define IOCTL_SEND_VIRQ			(0xf008)
#define IOCTL_CREATE_VIRTIO_DEVICE	(0xf009)
#define IOCTL_CREATE_VMCS		(0xf00a)
#define IOCTL_CREATE_VMCS_IRQ		(0xf00b)
#define IOCTL_UNREGISTER_VCPU		(0xf00c)

#endif
