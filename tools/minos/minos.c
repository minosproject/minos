#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <inttypes.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/ether.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/epoll.h>
#include <linux/netlink.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "minos.h"

static int map_guest_memory(int fd, uint64_t *offset, uint64_t *size)
{
	int ret, mem_fd;
	uint64_t args[2];

	args[0] = *offset;
	args[1] = *size;

	ret = ioctl(fd, MINOS_IOCTL_MMAP, args);
	if (ret)
		return ret;

	/* mmap the memory to userspace */
	mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (mem_fd < 0) {
		perror("/dev/mem");
		// ioctl umap guest mem
		return mem_fd;
	}

	*offset = (uint64_t)mmap(0, args[1], PROT_READ | PROT_WRITE,
			MAP_SHARED, mem_fd, args[0]);
	if (!(*offset)) {
		printf("mmap memory failed\n");
		return -EIO;
	}

	*size = args[1];
	return 0;
}

int main(int argc, char **argv)
{
	int fd, ret, vmid = -1;
	struct vm_info *vminfo;
	char name[32];
	uint64_t offset, size;

	fd = open("/dev/mvm/mvm0", O_RDWR | O_NONBLOCK);
	if (fd < 0) {
		perror("/dev/mvm/mvm0");
		return -EIO;
	}

	vminfo = malloc(sizeof(struct vm_info));
	if (!vminfo)
		return -ENOMEM;

	memset(vminfo, 0, sizeof(struct vm_info));
	strcpy(vminfo->name, "elinux");
	strcpy(vminfo->os_type, "linux");
	vminfo->nr_vcpus = 1;
	vminfo->bit64 = 1;
	vminfo->mem_start = VM_MEM_START;
	vminfo->mem_size = 128 * 1024 * 1024;
	vminfo->mem_end = vminfo->mem_start + vminfo->mem_size;
	vminfo->entry = 0x80080000;
	vminfo->setup_data = 0x80000000;

	vmid = ioctl(fd, MINOS_IOCTL_CREATE_VM, vminfo);
	if (vmid <= 0) {
		perror("vmid");
		return vmid;
	}

	close(fd);

	/* open the vm */
	memset(name, 0, 32);
	sprintf(name, "/dev/mvm/mvm%d", vmid);

	fd = open(name, O_RDWR | O_NONBLOCK);
	if (fd < 0) {
		perror(name);
		return -ENOENT;
	}

	offset = 0;
	size = 8 * 1024 * 1024;
	ret = map_guest_memory(fd, &offset, &size);
	if (ret) {
		printf("map guest memory failed\n");
		return ret;
	}

	printf("map guest memory successed 0x%llx 0x%x\n",
			offset, size);
	memset((void *)offset, 0, 4096);

	return 0;
}
