/*
 * BSD 3-Clause License
 *
 * Copyright (C) 2018 Min Le (lemin9538@gmail.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <getopt.h>
#include <pthread.h>
#include <sys/eventfd.h>

#include <mvm.h>
#include <vdev.h>
#include <mevent.h>

#define RECV_BUFFER_SIZE	(64)
#define MVM_NETLINK		(29)

struct vm *mvm_vm = NULL;

int verbose;

static struct sockaddr_nl src_addr, dest_addr;
static struct msghdr msg;
static char *dbuffer;
static struct iovec iov;

void *map_vm_memory(struct vm *vm)
{
	uint64_t args[2];
	void *addr = NULL;

	args[0] = 0;
	args[1] = vm->mem_size;

	addr = mmap(NULL, args[1], PROT_READ | PROT_WRITE,
			MAP_SHARED, vm->vm_fd, args[0]);
	if (addr == (void *)-1)
		return NULL;

	if (ioctl(vm->vm_fd, IOCTL_VM_MMAP, args)) {
		printf("* error - mmap memory failed 0x%lx 0x%lx\n",
				args[0], vm->mem_size);
		munmap(addr, vm->mem_size);
		addr = 0;
	}

	return addr;
}

static int create_new_vm(struct vm *vm)
{
	int fd, vmid = -1;
	struct vm_info info;

	strcpy(info.name, vm->name);
	strcpy(info.os_type, vm->os_type);
	info.nr_vcpus = vm->nr_vcpus;
	info.bit64 = vm->bit64;
	info.mem_size = vm->mem_size;
	info.mem_start = vm->mem_start;
	info.entry = vm->entry;
	info.setup_data = vm->setup_data;
	info.mmap_base = 0;

	fd = open("/dev/mvm/mvm0", O_RDWR | O_NONBLOCK);
	if (fd < 0) {
		perror("/dev/mvm/mvm0");
		return -EIO;
	}

	pr_debug("create new vm *\n");
	pr_debug("        -name       : %s\n", info.name);
	pr_debug("        -os_type    : %s\n", info.os_type);
	pr_debug("        -nr_vcpus   : %d\n", info.nr_vcpus);
	pr_debug("        -bit64      : %d\n", info.bit64);
	pr_debug("        -mem_size   : 0x%lx\n", info.mem_size);
	pr_debug("        -mem_start  : 0x%lx\n", info.mem_start);
	pr_debug("        -entry      : 0x%lx\n", info.entry);
	pr_debug("        -setup_data : 0x%lx\n", info.setup_data);

	vmid = ioctl(fd, IOCTL_CREATE_VM, &info);
	if (vmid <= 0) {
		perror("vmid");
		return vmid;
	}

	vm->hvm_paddr = info.mmap_base;
	close(fd);

	return vmid;
}

static int release_vm(int vmid)
{
	int fd, ret;

	fd = open("/dev/mvm/mvm0", O_RDWR);
	if (fd < 0)
		return -ENODEV;

	ret = ioctl(fd, IOCTL_DESTROY_VM, vmid);
	close(fd);

	return ret;
}

int destroy_vm(struct vm *vm)
{
	if (!vm)
		return -EINVAL;

	mevent_deinit();

	if (vm->nlh)
		free(vm->nlh);
	if (vm->sock_fd > 0)
		close(vm->sock_fd);

	vm->nlh = NULL;
	vm->sock_fd = -1;

	if (vm->mmap) {
		if (vm->vm_fd)
			ioctl(vm->vm_fd, IOCTL_VM_UNMAP, 0);
		munmap(vm->mmap, vm->mem_size);
	}

	if (vm->vm_fd > 0)
		close(vm->vm_fd);

	if (vm->vmid > 0)
		release_vm(vm->vmid);

	if (vm->image_fd > 0)
		close(vm->image_fd);

	if (vm->os_data > 0)
		free(vm->os_data);

	free(vm);
	mvm_vm = NULL;

	return 0;
}

void print_usage(void)
{
	fprintf(stderr, "\nUsage: mvm [options] \n\n");
	fprintf(stderr, "    -c <vcpu_count>            (set the vcpu numbers of the vm)\n");
	fprintf(stderr, "    -m <mem_size_in_MB>        (set the memsize of the vm - 2M align)\n");
	fprintf(stderr, "    -i <boot or kernel image>  (the kernel or bootimage to use)\n");
	fprintf(stderr, "    -s <mem_start>             (set the membase of the vm if not a boot.img)\n");
	fprintf(stderr, "    -n <vm name>               (the name of the vm)\n");
	fprintf(stderr, "    -t <vm type>               (the os type of the vm )\n");
	fprintf(stderr, "    -b <32 or 64>              (32bit or 64 bit )\n");
	fprintf(stderr, "    -r                         (do not load ramdisk image)\n");
	fprintf(stderr, "    -v                         (verbose print debug information)\n");
	fprintf(stderr, "    -d                         (run as a daemon process)\n");
	fprintf(stderr, "\n");
	exit(EXIT_FAILURE);
}

static int create_and_init_vm(struct vm *vm)
{
	int ret = 0;
	char path[32];

	if (vm->entry == 0)
		vm->entry = VM_MEM_START;
	if (vm->mem_start == 0)
		vm->mem_start = VM_MEM_START;
	if (vm->mem_size == 0)
		vm->mem_size = VM_MIN_MEM_SIZE;
	if (vm->nr_vcpus == 0)
		vm->nr_vcpus = 1;

	vm->vmid = create_new_vm(vm);
	if (vm->vmid <= 0)
		return (vm->vmid);

	memset(path, 0, 32);
	sprintf(path, "/dev/mvm/mvm%d", vm->vmid);
	vm->vm_fd = open(path, O_RDWR | O_NONBLOCK);
	if (vm->vm_fd < 0) {
		perror(path);
		return -EIO;
	}

	/*
	 * map a fix region for this vm, need to call ioctl
	 * to informe hypervisor to map the really physical
	 * memory
	 */
	vm->mmap = map_vm_memory(vm);
	if (!vm->mmap)
		return -EAGAIN;

	/* load the image into the vm memory */
	ret = vm->os->load_image(vm);
	if (ret)
		return ret;

	return 0;
}

static struct vm_os *get_vm_os(char *os_type)
{
	struct vm_os **os_start = (struct vm_os **)&__start_mvm_os;
	struct vm_os **os_end = (struct vm_os **)&__stop_mvm_os;
	struct vm_os *default_os = NULL;
	struct vm_os *os;

	for (; os_start < os_end; os_start++) {
		os = *os_start;
		printf("%s %s\n", os_type, os->name);
		if (strcmp(os_type, os->name) == 0)
			return os;

		if (strcmp("default", os->name) == 0)
			default_os = os;
	}

	return default_os;
}

static int mvm_netlink_init(struct vm *vm)
{
	int ret;
	int sz = 4 * 1024;
	int on = 1;

	vm->sock_fd = socket(AF_NETLINK, SOCK_RAW, MVM_NETLINK);
	if (vm->sock_fd < 0) {
		perror("can not open sock fd for slgps\n");
		return -EIO;
	}

	memset(&src_addr, 0, sizeof(src_addr));
	src_addr.nl_family = AF_NETLINK;
	src_addr.nl_pid = getpid();
	src_addr.nl_groups = 0;

	if (setsockopt(vm->sock_fd, SOL_SOCKET, SO_RCVBUFFORCE,
				&sz, sizeof(sz)) < 0) {
		printf("Unable to set event socket SO_RCVBUFFORCE\n");
		goto out;
	}

	if (setsockopt(vm->sock_fd, SOL_SOCKET, SO_PASSCRED,
				&on, sizeof(on)) < 0) {
		printf("Unable to set event socket SO_PASSCRED\n");
		goto out;
	}

	ret = bind(vm->sock_fd, (struct sockaddr *)&src_addr, sizeof(src_addr));
	if (ret) {
		perror("bind netlink faild\n");
		goto out;
	}

	vm->nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(RECV_BUFFER_SIZE));
	if (!vm->nlh) {
		perror("alloc netlink memory failed\n");
		goto out;
	}

	memset(&dest_addr, 0, sizeof(dest_addr));
	dest_addr.nl_family = AF_NETLINK;
	dest_addr.nl_pid = 0;
	dest_addr.nl_groups = 0;

	vm->nlh->nlmsg_len = RECV_BUFFER_SIZE;
	vm->nlh->nlmsg_pid = getpid();
	vm->nlh->nlmsg_flags = 0;

	iov.iov_base = (void *)vm->nlh;
	iov.iov_len = NLMSG_SPACE(RECV_BUFFER_SIZE);

	memset(&msg, 0, sizeof(msg));
	msg.msg_name = (void *)&dest_addr;
	msg.msg_namelen = sizeof(dest_addr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	memset(vm->nlh, 0, NLMSG_SPACE(RECV_BUFFER_SIZE));
	dbuffer = (char *)NLMSG_DATA(vm->nlh);

	return 0;

out:
	close(vm->sock_fd);
	vm->sock_fd = -1;
	return -EINVAL;
}

static int vm_rest_init(struct vm *vm)
{
	create_vdev(vm, "virtio_console", NULL);
	return 0;
}

static void mvm_handle_event(struct vdev *vdev)
{
	if (!vdev)
		return;

	vdev->ops->handle_event(vdev);
}

void *vm_rx_thread(void *data)
{
	int ret;
	unsigned long value;
	struct vm *vm = mvm_vm;

	while (1) {
		ret = recvmsg(vm->sock_fd, &msg, 0);;
		if ((ret <= 0) || (vm->nlh->nlmsg_len <= 0))
			continue;

		value = *(unsigned long *)dbuffer;
		mvm_handle_event((struct vdev *)value);
	}
}

static int mvm_main_loop(void)
{
	int ret;
	struct vm *vm = mvm_vm;
	pthread_t rx_thread;

	ret = mvm_netlink_init(vm);
	if (ret)
		return ret;

	/*
	 * create a thread to handle the netlink
	 * event reported by kernel, and those event
	 * are all irqed by guest vm
	 */
	ret = pthread_create(&rx_thread, NULL, vm_rx_thread, NULL);
	if (ret) {
		pr_err("create rm thread failed\n");
		return ret;
	}

	/* now start the vm */
	ret = ioctl(vm->vm_fd, IOCTL_POWER_UP_VM, NULL);
	if (ret)
		return ret;

	mevent_dispatch();

	return -EAGAIN;
}

static int mvm_main(struct vm *vm, char *image_path, unsigned long flags)
{
	int image_fd, ret;
	struct vm_os *os;

	if (vm->name[0] == 0)
		strcpy(vm->name, "unknown");
	if (vm->os_type[0] == 0)
		strcpy(vm->os_type, "unknown");

	os = get_vm_os(vm->os_type);
	if (!os)
		return -EINVAL;

	/* read the image to get the entry and other args */
	image_fd = open(image_path, O_RDWR | O_NONBLOCK);
	if (image_fd < 0) {
		free(vm);
		perror(image_path);
		return -ENOENT;
	}

	vm->os = os;
	vm->image_fd = image_fd;
	vm->flags = flags;
	init_list(&vm->vdev_list);

	/* free the unused memory */
	free(image_path);

	ret = os->early_init(vm);
	if (ret) {
		printf("* error - os early init faild %d\n", ret);
		goto release_vm;
	}

	/* ensure the below field is not modified */
	vm->vmid = 0;
	vm->vm_fd = -1;

	ret = create_and_init_vm(vm);
	if (ret)
		goto release_vm;

	/* io events init before vdev init */
	ret = mevent_init();
	if (ret)
		goto release_vm;

	ret = vm_rest_init(vm);
	if (ret)
		goto release_vm;

	ret = mvm_vm->os->setup_vm_env(vm);
	if (ret)
		return ret;

	mvm_main_loop();

release_vm:
	destroy_vm(mvm_vm);
	return ret;
}

static struct option options[] = {
	{"vcpu_number", required_argument, NULL, 'c'},
	{"mem_size",	required_argument, NULL, 'm'},
	{"image",	required_argument, NULL, 'i'},
	{"mem_start",	required_argument, NULL, 's'},
	{"name",	required_argument, NULL, 'n'},
	{"os_type",	required_argument, NULL, 't'},
	{"bit",		required_argument, NULL, 'b'},
	{"no_ramdisk",	no_argument,	   NULL, 'r'},
	{"help",	no_argument,	   NULL, 'h'},
	{NULL,		0,		   NULL,  0}
};

static int parse_vm_memsize(char *buf, uint64_t *size)
{
	int len = 0;

	if (!buf)
		return -EINVAL;

	len = strlen(buf) - 1;

	if ((buf[len] != 'm') && (buf[len] != 'M'))
		return -EINVAL;

	buf[len] = '\0';
	*size = atol(buf) * 1024 * 1024;

	return 0;
}

static int parse_vm_membase(char *buf, unsigned long *value)
{
	if (strlen(buf) < 3)
		return -EINVAL;

	if ((buf[0] == '0') && (buf[1] == 'x')) {
		sscanf(buf, "0x%lx", value);
		return 0;
	}

	return -EINVAL;
}

static void signal_handler(int signum)
{
	int vmid = 0xfff;

	switch (signum) {
	case SIGALRM:
	case SIGINT:
	case SIGTERM:
		if (mvm_vm)
			vmid = mvm_vm->vmid;

		printf("* received signal %i vm-%d\n", signum, vmid);
		destroy_vm(mvm_vm);
		exit(0);
	default:
		break;
	}
}

int main(int argc, char **argv)
{
	int ret, opt, idx;
	char *image_path = NULL;
	char *optstr = "c:m:i:s:n:t:b:rv?hd";
	unsigned long flags = 0;
	int run_as_daemon = 0;

	mvm_vm = (struct vm *)malloc(sizeof(struct vm));
	if (!mvm_vm)
		return -ENOMEM;

	/*
	 * default is 64 bit, 1 vcpus and 32M memory
	 */
	memset(mvm_vm, 0, sizeof(struct vm));
	mvm_vm->bit64 = 1;

	while ((opt = getopt_long(argc, argv, optstr, options, &idx)) != -1) {
		switch(opt) {
		case 'c':
			mvm_vm->nr_vcpus = atoi(optarg);
			break;
		case 'm':
			ret = parse_vm_memsize(optarg, &mvm_vm->mem_size);
			if (ret)
				print_usage();
			break;
		case 'i':
			image_path = malloc(256);
			if (!image_path) {
				free(mvm_vm);
				return -ENOMEM;
			}

			strcpy(image_path, optarg);
			break;
		case 's':
			ret = parse_vm_membase(optarg, &mvm_vm->mem_start);
			if (ret) {
				print_usage();
				free(mvm_vm);
				return -EINVAL;
			}
			break;
		case 'n':
			strncpy(mvm_vm->name, optarg, 31);
			break;
		case 't':
			strncpy(mvm_vm->os_type, optarg, 31);
			break;
		case 'b':
			ret = atoi(optarg);
			if ((ret != 32) && (ret != 64))
				print_usage();
			mvm_vm->bit64 = ret == 64 ? 1 : 0;
			break;
		case 'r':
			flags |= MVM_FLAGS_NO_RAMDISK;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'd':
			run_as_daemon = 1;
			break;
		case 'h':
			print_usage();
			free(mvm_vm);
			return 0;
		default:
			break;
		}
	}

	if (!image_path) {
		printf("* error - please point the image for this VM\n");
		return -1;
	}

	if (mvm_vm->nr_vcpus > VM_MAX_VCPUS) {
		printf("* warning - support max %d vcpus\n", VM_MAX_VCPUS);
		mvm_vm->nr_vcpus = VM_MAX_VCPUS;
	}

	if (run_as_daemon) {
		if (daemon(1, 1)) {
			printf("failed to run as daemon\n");
			exit(EXIT_FAILURE);
		}
	}

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	return mvm_main(mvm_vm, image_path, flags);
}
