#ifndef __MINOS_USER_H__
#define __MINOS_USER_H__

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <sys/uio.h>
#include <linux/netlink.h>

#include <compiler.h>
#include <vmcs.h>
#include <barrier.h>
#include <mvm_queue.h>
#include <debug.h>
#include <list.h>
#include <os.h>
#include <vm.h>

#define INVALID_MMAP_ADDR		((void *)-1)

#define MAXCOMLEN			(19)

#define PAGE_SIZE			(4096)

#define VM_MEM_START			(0x80000000UL)
#define VM_MIN_MEM_SIZE			(64 * 1024 * 1024)
#define MEM_BLOCK_SIZE			(0x200000)
#define MEM_BLOCK_SHIFT			(21)
#define VM_MAX_MMAP_SIZE		(MEM_BLOCK_SIZE * 8)

#define ALIGN(v, s)			((v) & ~((s) - 1))
#define BALIGN(v, s)			(((v) + (s) - 1) & ~((s) - 1))

#define MEM_BLOCK_ALIGN(v)		((v) & ~(MEM_BLOCK_SIZE - 1))
#define MEM_BLOCK_BALIGN(v) \
	(((v) + MEM_BLOCK_SIZE - 1) & ~(MEM_BLOCK_SIZE - 1))

#define VM_MAX_VCPUS			(8)

#define VMCS_SIZE(nr)			BALIGN(nr * sizeof(struct vmcs), PAGE_SIZE)

#endif
