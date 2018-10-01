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
#include <sys/prctl.h>

#include <mvm.h>
#include <vdev.h>
#include <mevent.h>

struct vm_info {
	char name[32];
	char os_type[32];
	int32_t nr_vcpus;
	int32_t bit64;
	uint64_t mem_size;
	uint64_t mem_start;
	uint64_t entry;
	uint64_t setup_data;
	uint64_t mmap_base;
};

#define VM_MAX_DEVICES	(10)

struct device_info {
	int nr_device;
	char *device_args[VM_MAX_DEVICES];
};

struct vm_config {
	unsigned long flags;
	struct vm_info vm_info;
	struct device_info device_info;
	char bootimage_path[256];
	char rootfs_path[256];
	char cmdline[256];
};

struct vm *mvm_vm = NULL;
static struct vm_config *global_config = NULL;

int verbose;

static void free_vm_config(struct vm_config *config);
int vm_shutdown(struct vm *vm);

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
		pr_err("mmap memory failed 0x%lx 0x%lx\n", args[0],
				vm->mem_size);
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

	pr_info("create new vm *\n");
	pr_info("        -name       : %s\n", info.name);
	pr_info("        -os_type    : %s\n", info.os_type);
	pr_info("        -nr_vcpus   : %d\n", info.nr_vcpus);
	pr_info("        -bit64      : %d\n", info.bit64);
	pr_info("        -mem_size   : 0x%lx\n", info.mem_size);
	pr_info("        -mem_start  : 0x%lx\n", info.mem_start);
	pr_info("        -entry      : 0x%lx\n", info.entry);
	pr_info("        -setup_data : 0x%lx\n", info.setup_data);

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
	int i;
	struct vdev *vdev;
	struct epoll_event ee;

	if (!vm)
		return -EINVAL;

	memset(&ee, 0, sizeof(struct epoll_event));

	if (vm->epfds && vm->eventfds) {
		for (i = 0; i < vm->nr_vcpus; i++) {
			if ((vm->epfds[i] > 0) && (vm->eventfds[i] > 0)) {
				ee.events = EPOLLIN;
				epoll_ctl(vm->epfds[i], EPOLL_CTL_DEL,
						vm->eventfds[i], &ee);
			}

			if (vm->eventfds[i] > 0) {
				close(vm->eventfds[i]);
				vm->eventfds[i] = -1;
			}

			if (vm->epfds[i] > 0) {
				close(vm->epfds[i]);
				vm->epfds[i] = -1;
			}
		}
	}

	if (vm->irqs) {
		for (i = 0; i < vm->nr_vcpus; i++) {
			if (vm->irqs[i] <= 0)
				continue;

			ioctl(vm->vm_fd, IOCTL_UNREGISTER_VCPU,
					(unsigned long)vm->irqs[i]);
		}
	}

	list_for_each_entry(vdev, &vm->vdev_list, list)
		release_vdev(vdev);

	mevent_deinit();

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

	free_vm_config(global_config);
	global_config = NULL;

	return 0;
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

		pr_info("received signal %i shutdown vm-%d\n", signum, vmid);
		vm_shutdown(mvm_vm);
		break;
	default:
		break;
	}

	exit(0);
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
	fprintf(stderr, "    -D                         (device argument)\n");
	fprintf(stderr, "    -C                         (set the cmdline for the os)\n");
	fprintf(stderr, "\n");
	exit(EXIT_FAILURE);
}

void *hvm_map_iomem(void *base, size_t size)
{
	void *iomem;
	int fd = open("/dev/mvm/mvm0", O_RDWR);

	if (fd < 0) {
		pr_err("open /dev/mvm/mvm0 failed\n");
		return (void *)-1;
	}

	iomem = mmap(NULL, size, PROT_READ | PROT_WRITE,
			MAP_SHARED, fd, (unsigned long)base);
	close(fd);

	return iomem;
}

static int vm_create_vmcs(struct vm *vm)
{
	int ret;
	void *vmcs;

	ret = ioctl(vm->vm_fd, IOCTL_CREATE_VMCS, &vmcs);
	if (ret)
		return ret;

	if (!vmcs)
		return -ENOMEM;

	vm->vmcs = hvm_map_iomem(vmcs, VMCS_SIZE(vm->nr_vcpus));
	if (vm->vmcs == (void *)-1)
		return -ENOMEM;

	return 0;
}

static int create_and_init_vm(struct vm *vm)
{
	int ret = 0;
	char path[32];

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

	ret = vm_create_vmcs(vm);
	if (ret)
		return -ENOMEM;

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
		if (strcmp(os_type, os->name) == 0)
			return os;

		if (strcmp("default", os->name) == 0)
			default_os = os;
	}

	return default_os;
}

static int vm_vdev_init(struct vm *vm, struct vm_config *config)
{
	int i;
	char *tmp, *pos;
	char *type = NULL, *arg = NULL;
	struct device_info *info = &config->device_info;

	for (i = 0; i < info->nr_device; i++) {
		tmp = info->device_args[i];
		if (!tmp || (strlen(tmp) == 0)) {
			pr_warn("invaild device argument index%d\n", i);
			continue;
		}

		pos = strchr(tmp, ',');
		if (pos == NULL) {
			pr_err("invaild device argumet %s", tmp);
			continue;
		}

		type = tmp;
		arg = pos + 1;
		*pos = '\0';

		if (create_vdev(vm, type, arg))
			pr_err("create %s-%s failed\n", type, arg);
	}

	return 0;
}

static void vmcs_ack(struct vmcs *vmcs)
{
	if (vmcs->guest_index == vmcs->host_index)
		return;

	vmcs->guest_index++;
	dmb();

	if (vmcs->guest_index < vmcs->host_index)
		pr_warn("something wrong or there are new message\n");
}

static int vcpu_handle_mmio(struct vm *vm, int trap_reason,
		uint64_t trap_data, uint64_t *trap_result)
{
	int ret = -EIO;
	struct vdev *vdev;

	list_for_each_entry(vdev, &vm->vdev_list, list) {
		if ((trap_data >= vdev->guest_iomem) &&
				(trap_data < vdev->guest_iomem + PAGE_SIZE)) {
			pthread_mutex_lock(&vdev->lock);
			ret = vdev->ops->handle_event(vdev, trap_reason,
					trap_data, trap_result);
			pthread_mutex_unlock(&vdev->lock);
		}
	}

	return ret;
}

static int vcpu_handle_common_trap(struct vm *vm, int trap_reason,
		uint64_t trap_data, uint64_t *trap_result)
{
	switch (trap_reason) {
	case VMTRAP_REASON_REBOOT:
	case VMTRAP_REASON_SHUTDOWN:
		mvm_queue_push(&vm->queue, trap_reason, NULL, 0);
		break;

	default:
		break;
	}

	return 0;
}

static void handle_vcpu_event(struct vmcs *vmcs)
{
	int ret;
	uint32_t trap_type = vmcs->trap_type;
	uint32_t trap_reason = vmcs->trap_reason;
	uint64_t trap_data = vmcs->trap_data;
	uint64_t trap_result = vmcs->trap_result;

	switch (trap_type) {
	case VMTRAP_TYPE_COMMON:
		ret = vcpu_handle_common_trap(mvm_vm, trap_reason,
				trap_data, &trap_result);
		break;

	case VMTRAP_TYPE_MMIO:
		ret = vcpu_handle_mmio(mvm_vm, trap_reason,
				trap_data, &trap_result);
		break;

	default:
		break;
	}

	vmcs->trap_ret = ret;
	vmcs->trap_result = trap_result;

	vmcs_ack(vmcs);
}

void *vm_vcpu_thread(void *data)
{
	int ret;
	int eventfd;
	int epfd;
	struct vm *vm = mvm_vm;
	struct epoll_event event;
	struct epoll_event ep_events;
	struct vmcs *vmcs;
	unsigned long i = (unsigned long)data;
	eventfd_t value;
	char buf[32];

	memset(buf, 0, 32);
	sprintf(buf, "vm%d-vcpu-event", vm->vmid);
	prctl(PR_SET_NAME, buf);

	if (i >= vm->nr_vcpus)
		return NULL;

	eventfd = vm->eventfds[i];
	epfd = vm->epfds[i];
	vmcs = (struct vmcs *)(vm->vmcs + i * sizeof(struct vmcs));

	if (eventfd <= 0 || epfd <= 0)
		return NULL;

	memset(&event, 0, sizeof(struct epoll_event));
	event.events = (unsigned long)EPOLLIN;
	event.data.fd = eventfd;
	ret = epoll_ctl(epfd, EPOLL_CTL_ADD, eventfd, &event);
	if (ret)
		return NULL;

	while (1) {
		ret = epoll_wait(epfd, &ep_events, 1, -1);
		if (ret <= 0) {
			pr_err("epoll failed for vcpu\n");
			continue;
		}

		eventfd_read(eventfd, &value);
		if (value > 1)
			pr_err("invaild vmcs state\n");

		handle_vcpu_event(vmcs);
	}

	return NULL;
}

int __vm_shutdown(struct vm *vm)
{
	pr_info("***************************\n");
	pr_info("vm-%d shutdown exit mvm\n", vm->vmid);
	pr_info("***************************\n");

	destroy_vm(vm);
	exit(0);
}

int vm_shutdown(struct vm *vm)
{
	int ret;

	ret = ioctl(vm->vm_fd, IOCTL_POWER_DOWN_VM, 0);
	if (ret) {
		pr_err("can not power-off vm-%d now, try again\n",
				vm->vmid);
		return -EAGAIN;
	}

	return __vm_shutdown(vm);
}

int __vm_reboot(struct vm *vm)
{
	int ret;
	struct vdev *vdev;

	/* hypervisor has been disable the vcpu */
	pr_info("***************************\n");
	pr_info("reboot the vm-%d\n", vm->vmid);
	pr_info("***************************\n");

	list_for_each_entry(vdev, &vm->vdev_list, list) {
		if (vdev->ops->reset)
			vdev->ops->reset(vdev);
	}

	/* load the image into the vm memory */
	ret = vm->os->load_image(vm);
	if (ret)
		return ret;

	ret = mvm_vm->os->setup_vm_env(vm, global_config->cmdline);
	if (ret)
		return ret;

	if (ioctl(vm->vm_fd, IOCTL_POWER_UP_VM, 0)) {
		pr_err("power up vm-%d failed\n", vm->vmid);
		return -EAGAIN;
	}

	return 0;
}

int vm_reboot(struct vm *vm)
{
	int ret;

	ret = ioctl(vm->vm_fd, IOCTL_RESTART_VM, 0);
	if (ret) {
		pr_err("can not reboot vm-%d now, try again\n",
				vm->vmid);
		return -EAGAIN;
	}

	return __vm_reboot(vm);
}

static void handle_vm_event(struct vm *vm, struct mvm_node *node)
{
	pr_info("handle vm event %d\n", node->type);

	switch (node->type) {
	case VMTRAP_REASON_REBOOT:
		__vm_reboot(vm);
		break;
	case VMTRAP_REASON_SHUTDOWN:
		__vm_shutdown(vm);
		break;
	default:
		pr_err("unsupport vm event %d\n", node->type);
		break;
	}

	mvm_queue_free(node);
}

static int mvm_main_loop(void)
{
	int ret, i, irq;
	struct vm *vm = mvm_vm;
	pthread_t vcpu_thread;
	int *base;
	uint64_t arg;
	struct mvm_node *node;

	/*
	 * create the eventfd and the epoll_fds for
	 * this vm
	 */
	base = (int *)malloc(sizeof(int) * vm->nr_vcpus * 3);
	if (!base)
		return -ENOMEM;

	memset(base, -1, sizeof(int) * vm->nr_vcpus * 3);
	vm->eventfds = base;
	vm->epfds = base + vm->nr_vcpus;
	vm->irqs = base + vm->nr_vcpus * 2;

	for (i = 0; i < vm->nr_vcpus; i++) {
		irq = ioctl(vm->vm_fd, IOCTL_CREATE_VMCS_IRQ, (unsigned long)i);
		if (irq < 0)
			return -ENOENT;

		vm->eventfds[i] = eventfd(0, 0);
		if (vm->eventfds[i] < 0)
			return -ENOENT;

		vm->epfds[i] = epoll_create(1);
		if (vm->epfds[i] < 0)
			return -ENOENT;

		/*
		 * register the irq and eventfd to kernel
		 */
		vm->irqs[i] = irq;
		arg = ((unsigned long)vm->eventfds[i] << 32) | irq;
		ret = ioctl(vm->vm_fd, IOCTL_REGISTER_VCPU, &arg);
		if (ret)
			return ret;

		ret = pthread_create(&vcpu_thread, NULL,
				vm_vcpu_thread, (void *)(unsigned long)i);
		if (ret) {
			pr_err("create vcpu thread failed\n");
			return ret;
		}
	}

	ret = pthread_create(&vcpu_thread, NULL, mevent_dispatch,
			(void *)(unsigned long)(vm->vmid));
	if (ret) {
		pr_err("create mevent thread failed\n");
		return ret;
	}

	/* now start the vm */
	ret = ioctl(vm->vm_fd, IOCTL_POWER_UP_VM, NULL);
	if (ret)
		return ret;

	for (;;) {
		/* here wait for the trap type for VM */
		node = mvm_queue_pop(&vm->queue);
		if (node == NULL) {
			pr_err("mvm queue is abnormal shutdown vm\n");
			vm_shutdown(vm);
			break;
		} else
			handle_vm_event(vm, node);
	}

	return -EAGAIN;
}

static int mvm_main(struct vm_config *config)
{
	int image_fd, ret;
	struct vm *vm;
	struct vm_os *os;
	struct vm_info *vm_info = &config->vm_info;

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	mvm_vm = vm = (struct vm *)malloc(sizeof(struct vm));
	if (!vm)
		return -ENOMEM;

	os = get_vm_os(vm_info->os_type);
	if (!os)
		return -EINVAL;

	/* read the image to get the entry and other args */
	image_fd = open(config->bootimage_path, O_RDWR | O_NONBLOCK);
	if (image_fd < 0) {
		free(vm);
		perror(config->bootimage_path);
		return -ENOENT;
	}

	/* udpate the vm from vm_info */
	vm->os = os;
	vm->bit64 = 1;
	vm->image_fd = image_fd;
	vm->flags = config->flags;
	vm->vmid = 0;
	vm->vm_fd = -1;
	vm->entry = vm_info->entry;
	vm->mem_start = vm_info->mem_start;
	vm->mem_size = vm_info->mem_size;
	vm->nr_vcpus = vm_info->nr_vcpus;
	strcpy(vm->name, vm_info->name);
	strcpy(vm->os_type, vm_info->os_type);
	init_list(&vm->vdev_list);

	ret = os->early_init(vm);
	if (ret) {
		pr_err("os early init faild %d\n", ret);
		goto release_vm;
	}

	if (vm->entry == 0)
		vm->entry = VM_MEM_START;
	if (vm->mem_start == 0)
		vm->mem_start = VM_MEM_START;
	if (vm->mem_size == 0)
		vm->mem_size = VM_MIN_MEM_SIZE;

	ret = create_and_init_vm(vm);
	if (ret)
		goto release_vm;

	mvm_queue_init(&vm->queue);

	/* io events init before vdev init */
	ret = mevent_init();
	if (ret)
		goto release_vm;

	ret = vm_vdev_init(vm, config);
	if (ret)
		goto release_vm;

	ret = mvm_vm->os->setup_vm_env(vm, config->cmdline);
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

static int add_device_info(struct device_info *dinfo, char *name)
{
	char *arg = NULL;

	if (dinfo->nr_device == VM_MAX_DEVICES) {
		pr_err("support max %d vdev\n", VM_MAX_DEVICES);
		return -EINVAL;
	}

	arg = calloc(1, strlen(name) + 1);
	if (!arg)
		return -ENOMEM;

	strcpy(arg, name);
	dinfo->device_args[dinfo->nr_device] = arg;
	dinfo->nr_device++;

	return 0;
}

static int check_vm_config(struct vm_config *config)
{
	if (strlen(config->bootimage_path) == 0) {
		pr_err("please point the image for this VM\n");
		return -EINVAL;
	}

	if (strlen(config->rootfs_path) == 0) {
		pr_info("no rootfs is point using ramdisk if exist\n");
		config->flags &= ~(MVM_FLAGS_NO_RAMDISK);
	}

	if (config->vm_info.nr_vcpus > VM_MAX_VCPUS) {
		pr_warn("support max %d vcpus\n", VM_MAX_VCPUS);
		config->vm_info.nr_vcpus = VM_MAX_VCPUS;
	}

	if (config->vm_info.name[0] == 0)
		strcpy(config->vm_info.name, "unknown");
	if (config->vm_info.os_type[0] == 0)
		strcpy(config->vm_info.os_type, "unknown");

	return 0;
}

static void free_vm_config(struct vm_config *config)
{
	int i;

	if (!config)
		return;

	for (i = 0; i < config->device_info.nr_device; i++) {
		if (!config->device_info.device_args[i])
			continue;
		free(config->device_info.device_args[i]);
	}

	free(config);
}

int main(int argc, char **argv)
{
	int ret, opt, idx;
	char *optstr = "c:C:m:i:s:n:D:t:b:rv?hd";
	int run_as_daemon = 0;
	struct vm_info *vm_info;
	struct device_info *device_info;

	global_config = calloc(1, sizeof(struct vm_config));
	if (!global_config)
		return -ENOMEM;

	vm_info = &global_config->vm_info;
	device_info = &global_config->device_info;

	while ((opt = getopt_long(argc, argv, optstr, options, &idx)) != -1) {
		switch(opt) {
		case 'c':
			vm_info->nr_vcpus = atoi(optarg);
			break;
		case 'm':
			ret = parse_vm_memsize(optarg, &vm_info->mem_size);
			if (ret)
				print_usage();
			break;
		case 'i':
			if (strlen(optarg) > 255) {
				pr_err("invaild boot_image path %s\n", optarg);
				ret = -EINVAL;
				goto exit;
			}

			strcpy(global_config->bootimage_path, optarg);
			break;
		case 's':
			ret = parse_vm_membase(optarg, &vm_info->mem_start);
			if (ret) {
				print_usage();
				ret = -EINVAL;
				goto exit;
			}
			break;
		case 'n':
			strncpy(vm_info->name, optarg, 31);
			break;
		case 't':
			strncpy(vm_info->os_type, optarg, 31);
			break;
		case 'b':
			ret = atoi(optarg);
			if ((ret != 32) && (ret != 64)) {
				free(global_config);
				print_usage();
			}
			vm_info->bit64 = ret == 64 ? 1 : 0;
			break;
		case 'r':
			global_config->flags |= MVM_FLAGS_NO_RAMDISK;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'd':
			run_as_daemon = 1;
			break;
		case 'D':
			add_device_info(device_info, optarg);
			break;
		case 'C':
			if (strlen(optarg) > 255) {
				pr_err("cmdline is too long\n");
				ret = -EINVAL;
				goto exit;
			}
			strcpy(global_config->cmdline, optarg);
			break;
		case 'h':
			free(global_config);
			print_usage();
			return 0;
		default:
			break;
		}
	}

	ret = check_vm_config(global_config);
	if (ret)
		goto exit;

	if (run_as_daemon) {
		if (daemon(1, 1)) {
			pr_err("failed to run as daemon\n");
			ret = -EFAULT;
			goto exit;
		}
	}

	ret = mvm_main(global_config);

exit:
	free_vm_config(global_config);
	global_config = NULL;
	exit(ret);
}
