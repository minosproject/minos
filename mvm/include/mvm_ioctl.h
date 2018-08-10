#ifndef __MINOS_IOCTL_H__
#define __MINOS_IOCTL_H__

#define IOCTL_CREATE_VM			(0xf000)
#define IOCTL_DESTROY_VM		(0xf001)
#define IOCTL_RESTART_VM		(0xf002)
#define IOCTL_POWER_DOWN_VM		(0xf003)
#define IOCTL_POWER_UP_VM		(0xf004)
#define IOCTL_VM_MMAP			(0xf005)
#define IOCTL_VM_UNMAP			(0xf006)
#define IOCTL_REGISTER_MDEV		(0xf007)
#define IOCTL_SEND_VIRQ			(0xf008)

#endif
