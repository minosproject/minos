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

#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/ether.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/epoll.h>
#include <linux/netlink.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <pthread.h>
#include <sys/eventfd.h>
#include <sys/prctl.h>
#include <time.h>

#include <minos/vm.h>
#include <minos/vdev.h>
#include <minos/mevent.h>
#include <minos/option.h>

int debug_enable;
struct vm *mvm_vm = NULL;

int vm_shutdown(struct vm *vm);
void *vm_vcpu_thread(void *data);

#ifdef __ANDROID__
#define DEV_MVM0	"/dev/mvm0"
#define DEV_MVM_PATH	"/dev/mvm"
#else
#define DEV_MVM0	"/dev/mvm/mvm0"
#define DEV_MVM_PATH	"/dev/mvm/mvm"
#endif

void *map_vm_memory(struct vm *vm)
{
	void *addr;
	uint64_t args[2];

	/*
	 * use vm->map_xxx as the default map config, if
	 * these two member has not been set, the config
	 * is as same as vm->map_xxx
	 */
	if (vm->map_size == 0) {
		vm->map_start = vm->mem_start;
		vm->map_size = vm->mem_size;
	}

	args[0] = vm->map_start;
	args[1] = vm->map_size;

	if (ioctl(vm->vm_fd, IOCTL_VM_MMAP, args)) {
		pr_err("mmap memory failed 0x%"PRIx64" 0x%"PRIx64"\n",
				vm->mem_start, vm->mem_size);
		return NULL;
	}

	addr = mmap64(NULL, (size_t)vm->map_size, PROT_READ | PROT_WRITE,
			MAP_SHARED, vm->vm_fd, vm->map_start);
	if (addr == (void *)-1) {
		pr_err("mmap vm memory failed 0x%lx\n", (unsigned long)addr);
		return NULL;
	}

	pr_notice("vm-%d 0x%"PRIx64"@0x%"PRIx64" mmap to 0x%lx\n",
			vm->vmid, vm->map_start, vm->map_size,
			(unsigned long)addr);

	return addr;
}

static int create_new_vm(struct vm *vm)
{
	int vmid = -1;
	struct vmtag info;

	strcpy(info.name, vm->name);
	strcpy(info.os_type, vm->os->name);
	info.nr_vcpu = vm->nr_vcpus;
	info.mem_size = vm->mem_size;
	info.mem_base = vm->mem_start;
	info.entry = vm->entry;
	info.setup_data = vm->setup_data;
	info.flags = vm->flags;
	info.vmid = -1;

	pr_notice("create new vm *\n");
	pr_notice("        -name       : %s\n", info.name);
	pr_notice("        -os_type    : %s\n", info.os_type);
	pr_notice("        -nr_vcpu    : %d\n", info.nr_vcpu);
	pr_notice("        -bit64      : %d\n", !(vm->flags & VM_FLAGS_32BIT));
	pr_notice("        -mem_size   : 0x%"PRIx64"\n", info.mem_size);
	pr_notice("        -mem_base  : 0x%"PRIx64"\n", info.mem_base);
	pr_notice("        -entry      : 0x%"PRIx64"\n", info.entry);
	pr_notice("        -setup_data : 0x%"PRIx64"\n", info.setup_data);

	vmid = ioctl(vm->vm0_fd, IOCTL_CREATE_VM, (unsigned long)&info);
	if (vmid <= 0) {
		pr_err("create new vm failed %d\n", vmid);
		goto out;
	}

out:
	return vmid;
}

static void vm_release_vcpu(struct vcpu *vcpu)
{
	struct epoll_event ee;

	memset(&ee, 0, sizeof(struct epoll_event));

	if ((vcpu->epoll_fd > 0) && (vcpu->event_fd > 0)) {
		ee.events = EPOLLIN;
		epoll_ctl(vcpu->epoll_fd, EPOLL_CTL_DEL, vcpu->event_fd, &ee);
	}

	if (vcpu->event_fd > 0) {
		close(vcpu->event_fd);
		vcpu->event_fd = -1;
	}

	if (vcpu->epoll_fd > 0) {
		close(vcpu->epoll_fd);
		vcpu->epoll_fd = -1;
	}

	if (vcpu->vcpu_irq)
		ioctl(vcpu->vm->vm_fd, IOCTL_UNREGISTER_VCPU, vcpu->vcpu_irq);

	free(vcpu);
}

static int destroy_vm(struct vm *vm)
{
	int i;

	mvm_vm = NULL;
	mvm_free_options();

	if (!vm)
		return -EINVAL;

	if (vm->vcpus) {
		for (i = 0; i < vm->nr_vcpus; i++) {
			if (vm->vcpus[i]) {
				pr_info("release vcpu-%d\n", i);
				vm_release_vcpu(vm->vcpus[i]);
			}
		}
		free(vm->vcpus);
	}

	release_vdevs(vm);

	pr_info("mevent deinit\n");
	mevent_deinit();

	if (vm->vm0_fd) {
		pr_info("close vm0 fd\n");
		close(vm->vm0_fd);
	}

	if (vm->vm_fd > 0) {
		pr_info("close vm fd\n");
		close(vm->vm_fd);
	}

	if (vm->image_fd > 0) {
		pr_info("close bootimage fd\n");
		close(vm->image_fd);
	}

	if (vm->kfd > 0) {
		pr_info("close kernel fd\n");
		close(vm->kfd);
	}

	if (vm->rfd > 0) {
		pr_info("close ramdisk fd\n");
		close(vm->rfd);
	}

	if (vm->dfd > 0) {
		pr_info("close dtb fd\n");
		close(vm->rfd);
	}

	if (vm->os && vm->os->vm_exit) {
		pr_info("do os exiting\n");
		vm->os->vm_exit(vm);
	}

	free(vm);

	return 0;
}

static void signal_handler(int signum)
{
	struct vm *vm;

	pr_notice("recevied signal %i\n", signum);

	switch (signum) {
	case SIGTERM:
	case SIGBUS:
	case SIGKILL:
	case SIGSEGV:
	case SIGSTOP:
	case SIGTSTP:
		if (mvm_vm) {
			vm = mvm_vm;
			mvm_vm = NULL;
			pr_notice("shutdown vm-%d\n", vm->vmid);
			vm_shutdown(vm);
		}
		break;
	default:
		break;
	}

	exit(0);
}

void *hvm_map_iomem(unsigned long base, size_t size)
{
	/*
	 * On 32-bit Android, off_t is a signed 32-bit integer. This
	 * limits functions that use off_t to working on files no
	 * larger than 2GiB. need to use mmap64
	 *
	 * also see:
	 * https://android.googlesource.com/platform/bionic/+/master/docs/32-bit-abi.md
	 */
	return mmap64(NULL, size, PROT_READ | PROT_WRITE,
			MAP_SHARED, mvm_vm->vm0_fd, base);
}

static void *vm_create_vmcs(struct vm *vm)
{
	int ret;
	uint64_t vmcs;

	ret = ioctl(vm->vm_fd, IOCTL_CREATE_VMCS, &vmcs);
	if (ret)
		return (void *)-1;

	return hvm_map_iomem((unsigned long)vmcs, VMCS_SIZE(vm->nr_vcpus));
}

static int vm_create_vcpus(struct vm *vm)
{
	int i, ret;
	void *vmcs;
	struct vcpu *vcpu;
	uint32_t arg[2];

	vmcs = vm_create_vmcs(vm);
	if (vmcs == (void *)-1) {
		pr_err("create VMCS failed\n");
		return -ENOMEM;
	}

	vm->vcpus = calloc(vm->nr_vcpus, sizeof(struct vcpu *));
	if (!vm->vcpus)
		return -ENOMEM;

	for (i = 0; i < vm->nr_vcpus; i++) {
		vcpu = calloc(1, sizeof(struct vcpu));
		if (!vcpu)
			return -ENOMEM;

		vcpu->cpuid = i;
		vcpu->vm = vm;
		vcpu->vmcs = vmcs + i * VMCS_ENTRY_SIZE;

		vcpu->vcpu_irq = ioctl(vm->vm_fd, IOCTL_CREATE_VMCS_IRQ, i);
		if (vcpu->vcpu_irq < 0) {
			pr_err("allocate virq for vmcs failed\n");
			return -ENOENT;
		}

		vcpu->event_fd = eventfd(0, 0);
		if (vcpu->event_fd < 0) {
			pr_err("get event fd fail\n");
			return -ENOENT;
		}

		vcpu->epoll_fd = epoll_create(1);
		if (vcpu->epoll_fd < 0) {
			pr_err("create epoll fd fail\n");
			return -ENOENT;
		}

		arg[0] = vcpu->event_fd;
		arg[1] = vcpu->vcpu_irq;
		ret = ioctl(vm->vm_fd, IOCTL_REGISTER_VCPU, arg);
		if (ret) {
			pr_err("register vcpu fail\n");
			return ret;
		}

		ret = pthread_create(&vcpu->vcpu_thread, NULL,
				vm_vcpu_thread, (void *)(unsigned long)i);
		if (ret) {
			pr_err("create vcpu thread fail\n");
			return ret;
		}

		vm->vcpus[i] = vcpu;
	}

	return 0;
}

static int vm_create_resource(struct vm *vm)
{
	return ioctl(vm->vm_fd, IOCTL_CREATE_VM_RESOURCE, NULL);
}

static int vm_load_images(struct vm *vm)
{
	return os_load_images(vm);
}

static int create_and_init_vm(struct vm *vm)
{
	char path[32];
	int retry;

	vm->vm0_fd = open(DEV_MVM0, O_RDWR | O_NONBLOCK);
	if (vm->vm0_fd <= 0) {
		pr_err("open VM0 file fail %d\n", vm->vm0_fd);
		return -EIO;
	}

	vm->vmid = create_new_vm(vm);
	if (vm->vmid <= 0)
		return -EFAULT;

	memset(path, 0, sizeof(path));
	sprintf(path, DEV_MVM_PATH"%d", vm->vmid);

	for (retry = 0; retry < 5; retry++) {
		vm->vm_fd = open(path, O_RDWR | O_NONBLOCK);
		if (vm->vm_fd < 0)
			pr_err("fail to open %s %d\n", path, vm->vm_fd);
		else
			break;
	}

	if (vm->vm_fd < 0)
		return -ENOENT;

	return 0;
}

static void vmcs_ack(struct vmcs *vmcs)
{
	if (vmcs->guest_index == vmcs->host_index)
		return;

	vmcs->guest_index++;
	wmb();
}

static int vcpu_handle_mmio(struct vm *vm, int trap_reason,
		uint64_t trap_data, uint64_t *trap_result)
{
	int ret = -EIO;
	struct vdev *vdev;
	size_t size;
	uint64_t base;

	list_for_each_entry(vdev, &vm->vdev_list, list) {
		base = vdev->guest_iomem;
		size = vdev->iomem_size;
		if ((trap_data >= base) && (trap_data < base + size)) {
			pthread_mutex_lock(&vdev->lock);
			ret = vdev->ops->event(vdev, trap_reason,
					trap_data, trap_result);
			pthread_mutex_unlock(&vdev->lock);

			return ret;
		}
	}

	return -ENODEV;
}

static int vcpu_handle_common_trap(struct vm *vm, int trap_reason,
		uint64_t trap_data, uint64_t *trap_result)
{
	switch (trap_reason) {
	case VMTRAP_REASON_REBOOT:
	case VMTRAP_REASON_SHUTDOWN:
	case VMTRAP_REASON_WDT_TIMEOUT:
		mvm_queue_push(&vm->queue, trap_reason, NULL, 0);
		break;
	case VMTRAP_REASON_VM_SUSPEND:
		vm->state = VM_STAT_SUSPEND;
		pr_notice("vm-%d is suspend\n", vm->vmid);
		break;
	case VMTRAP_REASON_VM_RESUMED:
		vm->state = VM_STAT_RUNNING;
		pr_notice("vm-%d is resumed\n", vm->vmid);
		break;
	case VMTRAP_REASON_GET_TIME:
		*trap_result = (uint64_t)time(NULL);
		break;
	default:
		break;
	}

	return 0;
}

static void handle_vcpu_event(struct vmcs *vmcs)
{
	int ret = 0;
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
	eventfd_t value;
	char buf[32];
	struct vcpu *vcpu;
	unsigned long i = (unsigned long)data;

	memset(buf, 0, 32);
	sprintf(buf, "vm%d-vcpu-event", vm->vmid);
	prctl(PR_SET_NAME, buf);

	if (i >= vm->nr_vcpus)
		return NULL;

	vcpu = vm->vcpus[i];
	eventfd = vcpu->event_fd;
	epfd = vcpu->epoll_fd;
	vmcs = vcpu->vmcs;

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
			break;
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
	pr_notice("***************************\n");
	pr_notice("vm-%d shutdown exit mvm\n", vm->vmid);
	pr_notice("***************************\n");

	destroy_vm(vm);
	exit(0);
}

int vm_shutdown(struct vm *vm)
{
	int ret;

	if (vm && vm->vm_fd) {
		ret = ioctl(vm->vm_fd, IOCTL_POWER_DOWN_VM, 0);
		if (ret) {
			pr_err("can not power-off vm-%d now, try again\n",
					vm->vmid);
			return -EAGAIN;
		}
	}

	return __vm_shutdown(vm);
}

int __vm_reboot(struct vm *vm)
{
	int ret;
	struct vdev *vdev;

	/* hypervisor has been disable the vcpu */
	pr_notice("***************************\n");
	pr_notice("reboot the vm-%d\n", vm->vmid);
	pr_notice("***************************\n");

	list_for_each_entry(vdev, &vm->vdev_list, list) {
		if (vdev->ops->reset)
			vdev->ops->reset(vdev);
	}

	/* load the image into the vm memory */
	ret = vm->os->load_image(vm);
	if (ret)
		return ret;

	ret = os_setup_vm(vm);
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
	pr_notice("handle vm event %d\n", node->type);

	switch (node->type) {
	case VMTRAP_REASON_WDT_TIMEOUT:
		pr_err("vm-%d watchdog timeout reboot vm\n", vm->vmid);
		vm_reboot(vm);
		break;
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

static int mvm_main_loop(struct vm *vm)
{
	int ret;
	pthread_t vcpu_thread;
	struct mvm_node *node;

	ret = pthread_create(&vcpu_thread, NULL,
			mevent_dispatch, (void *)vm);
	if (ret) {
		pr_err("create mevent thread failed\n");
		return ret;
	}

	/* start the vm */
	ret = ioctl(vm->vm_fd, IOCTL_POWER_UP_VM, NULL);
	if (ret)
		return ret;

	for (;;) {
		/* here wait for the trap type for VM */
		node = mvm_queue_pop(&vm->queue);
		if (node == NULL) {
			pr_err("mvm queue is abnormal shutdown vm\n");
			break;
		} else {
			handle_vm_event(vm, node);
		}
	}

	return -EAGAIN;
}

static int mvm_check_vm_config(struct vm *vm)
{
	if (vm->flags & VM_FLAGS_NO_BOOTIMAGE) {
		if ((vm->kfd <= 0) || (vm->dfd <= 0)) {
			pr_err("no kernel and dtb image found\n");
			return -EINVAL;
		}
	} else {
		if (vm->image_fd <= 0) {
			pr_err("no bootimage\n");
			return -EINVAL;
		}
	}

	if (vm->name[0] == 0)
		strncpy(vm->name, "unknown", 8);

	if (!vm->os) {
		pr_err("no operation system found for this vm\n");
		return -EINVAL;
	}

	return 0;
}

static int mvm_main(void)
{
	int ret;
	struct vm *vm;

	signal(SIGTERM, signal_handler);
	signal(SIGBUS, signal_handler);
	signal(SIGKILL, signal_handler);
	signal(SIGSEGV, signal_handler);
	signal(SIGSTOP, signal_handler);
	signal(SIGTSTP, signal_handler);

	vm = mvm_vm = (struct vm *)calloc(1, sizeof(struct vm));
	if (!vm)
		return -ENOMEM;

	/*
	 * init some member to default value
	 */
	init_list(&vm->vdev_list);
	vm->mem_start = VM_MEM_START;
	vm->mem_size = VM_MIN_MEM_SIZE;
	vm->flags |= VM_FLAGS_NO_BOOTIMAGE | VM_FLAGS_DYNAMIC_AFF;

	/*
	 * get all the necessary vm options for this VM, then
	 * check the whether this VM has been config correctly
	 */
	ret = mvm_parse_option_group(OPTION_GRP_VM, vm);
	if (ret)
		goto error_option;

	ret = mvm_check_vm_config(vm);
	if (ret) {
		pr_err("option checking fail\n");
		goto error_option;
	}

	ret = os_early_init(vm);
	if (ret) {
		pr_err("os early init failed %d\n", ret);
		goto error_option;
	}

	mvm_queue_init(&vm->queue);

	if (mevent_init()) {
		pr_err("mevent init fail\n");
		goto error_option;
	}

	ret = create_and_init_vm(vm);
	if (ret) {
		pr_err("failed to create vm %d\n", ret);
		goto error_option;
	}

	/*
	 * create the device which is passed by the
	 * argument
	 */
	os_create_resource(vm);
	mvm_parse_option_group(OPTION_GRP_VDEV, vm);

	ret = vm_create_vcpus(vm);
	if (ret) {
		pr_err("create vcpus for vm failed %d\n", ret);
		goto error_out;
	}

	/*
	 * map a fix region for this vm, need to call ioctl
	 * to informe hypervisor to map the really physical
	 * memory
	 */
	vm->mmap = map_vm_memory(vm);
	if (!vm->mmap) {
		pr_err("map vm memory space to process failed\n");
		goto error_out;
	}

	/* load the image into the vm memory */
	ret = vm_load_images(vm);
	if (ret) {
		pr_err("load image for VM failed\n");
		goto error_out;
	}

	ret = os_setup_vm(vm);
	if (ret) {
		pr_err("setup vm fail\n");
		goto error_out;
	}

	ret = vm_create_resource(vm);
	if (ret) {
		pr_err("create resource for vm failed\n");
		goto error_out;
	}

	ret = mvm_main_loop(vm);
	if (mvm_vm == NULL)
		return 0;

error_out:
	mvm_vm = NULL;
	destroy_vm(vm);
	return ret;

error_option:
	mvm_vm = NULL;
	free(vm);
	return -EINVAL;
}

int main(int argc, char **argv)
{
	int ret;
	int run_as_daemon;

	ret = mvm_option_init(argc, argv);
	if (ret) {
		pr_err("invaild options %d\n", ret);
		goto out;
	}

	mvm_parse_option_bool("run_as_daemon", &run_as_daemon);
	if (run_as_daemon) {
		if (daemon(1, 1)) {
			pr_err("failed to run as daemon\n");
			goto out;
		}
	}

	mvm_parse_option_bool("debug_enable", &debug_enable);
	debug_enable = 1;

	ret = mvm_main();
out:
	mvm_free_options();
	return ret;
}
