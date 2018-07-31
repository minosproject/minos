#ifndef __LINUX_MINOS_H__
#define __LINUX_MINOS_H_

#include <linux/types.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/list.h>
#include <linux/fs.h>

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
#define HVC_VM_MMAP			HVC_VM_FN(5)
#define HVC_VM_UNMAP			HVC_VM_FN(6)
#define HVC_VM_GET_MMAP_INFO		HVC_VM_FN(7)

#define IOCTL_CREATE_VM			(0xf000)
#define IOCTL_DESTORY_VM		(0xf001)
#define IOCTL_RESTART_VM		(0xf002)
#define IOCTL_POWER_DOWN_VM		(0xf003)
#define IOCTL_POWER_UP_VM		(0xf004)
#define IOCTL_VM_MMAP			(0xf005)
#define IOCTL_VM_UNMAP			(0xf006)
#define IOCTL_VM_MMAP_INFO		(0xf007)

#define IOCTL_GET_VM_MAP_SIZE		(0xf100)

#define MINOS_VM_MAX			(64)

struct vm_info {
	int8_t name[32];
	int8_t os_type[32];
	int32_t nr_vcpus;
	int32_t bit64;
	uint64_t mem_size;
	uint64_t mem_start;
	uint64_t mem_end;
	uint64_t entry;
	uint64_t setup_data;
};

struct vm_device {
	int vmid;
	atomic_t opened;
	phys_addr_t pmem_map;
	void __iomem *vmem_map;
	unsigned long map_size;
	struct list_head list;
	struct vm_info vm_info;
	struct device *parent;
	struct device device;
	struct file_operations *fops;
};

extern unsigned long __minos_hvc(uint32_t id, unsigned long a0,
		unsigned long a1, unsigned long a2, unsigned long a3,
		unsigned long a4, unsigned long a5);

extern void minos_hvc_result1(void *x1);
extern void minos_hvc_result2(void *x1, void *x2);

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

static inline int hvc_vm_create(struct vm_info *vminfo)
{
	return minos_hvc1(HVC_VM_CREATE, vminfo);
}

static inline int hvc_vm_destory(int vmid)
{
	return minos_hvc1(HVC_VM_DESTORY, vmid);
}

static inline int hvc_vm_power_up(int vmid)
{
	return minos_hvc1(HVC_VM_POWER_UP, vmid);
}

static inline int hvc_vm_power_down(int vmid)
{
	return minos_hvc1(HVC_VM_POWER_DOWN, vmid);
}

static inline int hvc_vm_mmap(int vmid, uint64_t offset, uint64_t size)
{
	return minos_hvc3(HVC_VM_MMAP, vmid, offset, size);
}

static inline void hvc_vm_unmap(int vmid)
{
	minos_hvc1(HVC_VM_UNMAP, vmid);
}

static inline void *hvc_get_mmap_info(int vmid, unsigned long *size)
{
	void *ret;

	ret = (void *)minos_hvc1(HVC_VM_GET_MMAP_INFO, vmid);
	minos_hvc_result1(size);

	return ret;
}

#endif
