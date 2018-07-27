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
#include <getopt.h>

#include <mvm/mvm.h>
#include <mvm/bootimage.h>
#include <libfdt/libfdt.h>

static int no_ramdisk = 0;

void *map_vm_memory(int fd, uint64_t offset, uint64_t *size)
{
	int ret, mem_fd;
	uint64_t args[2];
	void *vbase;

	args[0] = offset;
	args[1] = *size;

	ret = ioctl(fd, MINOS_IOCTL_MMAP, args);
	if (ret)
		return NULL;

	/* mmap the memory to userspace */
	mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (mem_fd < 0) {
		perror("/dev/mem");
		// ioctl umap guest mem
		return NULL;
	}

	vbase = mmap(0, args[1], PROT_READ | PROT_WRITE,
			MAP_HUGETLB, mem_fd, args[0]);
	if (!vbase) {
		printf("mmap memory failed\n");
		return NULL;
	}

	*size = args[1];
	return vbase;
}

int create_new_vm(struct vm_info *vminfo)
{
	int fd, vmid = -1;

	fd = open("/dev/mvm/mvm0", O_RDWR | O_NONBLOCK);
	if (fd < 0) {
		perror("/dev/mvm/mvm0");
		return -EIO;
	}

	vmid = ioctl(fd, MINOS_IOCTL_CREATE_VM, vminfo);
	if (vmid <= 0) {
		perror("vmid");
		return vmid;
	}

	close(fd);

	return vmid;
}

int destory_vm(int vmid)
{
	return 0;
}

void print_usage(void)
{
	fprintf(stderr, "\nUsage: mvm [options] \n\n");
	fprintf(stderr, "    -c <vcpu_count>		(set the vcpu numbers of the vm)\n");
	fprintf(stderr, "    -m <mem_size_in_MB>	(set the memsize of the vm - 2M align)\n");
	fprintf(stderr, "    -i <boot or kernel image>  (the kernel or bootimage to use)\n");
	fprintf(stderr, "    -s <mem_start>		(set the membase of the vm if not a boot.img)\n");
	fprintf(stderr, "    -n <vm name>		(the name of the vm)\n");
	fprintf(stderr, "    -t <vm type>		(the os type of the vm )\n");
	fprintf(stderr, "    -b <32 or 64>		(32bit or 64 bit )\n");
	fprintf(stderr, "    -r				(do not load ramdisk image)\n");
	fprintf(stderr, "\n");
	exit(EXIT_FAILURE);
}

static int load_image(int vm_fd, int image_fd, uint32_t load_offset,
		uint32_t image_offset, uint64_t mem_offset, uint32_t load_size)
{
	int ret;
	uint32_t w_size;
	void *vbase;
	uint64_t offset = mem_offset, size;

	size = VM_MAX_MMAP_SIZE;

	if (lseek(image_fd, image_offset, SEEK_SET) == -1)
		return -EIO;

	while (load_size > 0) {
		vbase = map_vm_memory(vm_fd, offset, &size);
		if (vbase == 0) {
			printf("map guest memory failed\n");
			return -ENOMEM;
		}

		printf("map guest memory successed 0x%lx 0x%lx\n",
				(unsigned long)vbase, size);

		w_size = load_size > size ? size : load_size;
		ret = read(image_fd, (char *)(vbase + load_offset), w_size);
		if (ret != w_size)
			return -EIO;

		load_size -= w_size;
		offset += size;
		load_offset = 0;
		printf("load image 0x%x size remain:0x%x",
				w_size, load_size);
	}

	return 0;
}

static int load_boot_image(int vm_fd, int image_fd,
		struct vm_info *info, boot_img_hdr *hdr)
{
	int ret;
	uint64_t offset;
	uint32_t load_size, load_offset, seek, tmp;

	/*
	 * map vm memory to vm0 space and using mmap
	 * to map them into vm0 userspace, 16M is the
	 * max map size supported now.
	 */

	/* load the kernel image to guest memory */
	offset = hdr->kernel_addr - info->mem_start;
	offset = MEM_BLOCK_ALIGN(offset);
	load_offset = hdr->kernel_addr - offset;
	load_size = hdr->kernel_size;
	tmp = 1;
	seek = tmp* hdr->page_size;

	printf("load kernel image: 0x%lx 0x%x 0x%x 0x%x\n",
			offset, load_offset, seek, load_size);

	ret = load_image(vm_fd, image_fd, load_offset,
			seek, offset, load_size);
	if (ret) {
		printf("load kernel image failed\n");
		return ret;
	}

	tmp += (hdr->kernel_size + hdr->page_size - 1) / hdr->page_size;

	/* load the ramdisk image */
	if (no_ramdisk)
		goto load_dtb_image;

	offset = hdr->ramdisk_addr - info->mem_start;
	offset = MEM_BLOCK_ALIGN(offset);
	load_offset = hdr->ramdisk_addr - offset;
	load_size = hdr->ramdisk_size;
	seek = tmp * hdr->page_size;

	printf("load ramdisk image: 0x%lx 0x%x 0x%x 0x%x\n",
			offset, load_offset, seek, load_size);
	ret = load_image(vm_fd, image_fd, load_offset,
			seek, offset, load_size);
	if (ret) {
		printf("load ramdisk image failed\n");
		return ret;
	}

load_dtb_image:
	tmp += (hdr->ramdisk_size + hdr->page_size - 1) / hdr->page_size;
	offset = hdr->ramdisk_addr - info->mem_start;
	offset = MEM_BLOCK_ALIGN(offset);
	load_offset = hdr->second_addr - offset;
	load_size = hdr->second_size;
	seek = tmp * hdr->page_size;

	printf("load dtb image: 0x%lx 0x%x 0x%x 0x%x\n",
			offset, load_offset, seek, load_size);
	ret = load_image(vm_fd, image_fd, load_offset,
			seek, offset, load_size);
	if (ret) {
		printf("load dtb image failed\n");
		return ret;
	}

	return 0;
}

static int dts_setup_commandline(void *dtb, char *cmdline)
{
	int nodeoffset;

	nodeoffset = fdt_path_offset(dtb, "/chosen");
	if (nodeoffset < 0) {
		nodeoffset = fdt_add_subnode(dtb, 0, "chosen");
		if (nodeoffset < 0)
			return nodeoffset;
	}

	fdt_setprop(dtb, nodeoffset, "bootargs",
			cmdline, strlen(cmdline) + 1);

	return 0;
}

static int dts_setup_cpu(void *dtb, int vcpus)
{
	return 0;
}

static int dts_setup_memory(void *dtb, uint64_t mstart,
		uint64_t msize, int bit64)
{
	int offset;

	offset = fdt_path_offset(dtb, "/memory");
	if (offset < 0) {
		offset = fdt_add_subnode(dtb, 0, "memory");
		if (offset < 0)
			return offset;
	}

	return 0;
}

static int setup_vm_dts(int vm_fd, struct vm_info *info, boot_img_hdr *hdr)
{
	uint32_t dtb_hear_offset;
	void *vbase;
	uint64_t size;
	uint32_t offset, dtb_size, dtb_addr;

	dtb_size = hdr->second_size;
	dtb_addr = hdr->second_addr;

	offset = dtb_addr - info->mem_start;
	offset = MEM_BLOCK_ALIGN(offset);
	dtb_hear_offset = dtb_addr - offset;
	size = MEM_BLOCK_BALIGN(dtb_size);

	vbase = map_vm_memory(vm_fd, offset, &size);
	if (vbase == 0)
		return -ENOMEM;

	vbase += dtb_hear_offset;

	printf("checking DTB header at 0x%lx\n", (unsigned long)vbase);

	if (fdt_check_header(vbase)) {
		printf("invalid DTB please check the bootimage\n");
		return -EINVAL;
	}

	/*
	 * 1: set up commandline
	 * 2: set up nr vcpu
	 * 3: setup mem attr
	 */
	dts_setup_commandline(vbase, (char *)hdr->cmdline);
	dts_setup_cpu(vbase, info->nr_vcpus);
	dts_setup_memory(vbase, info->mem_start,
			info->mem_size, info->bit64);

	return 0;
}

static int main_loop(struct vm_info *info, char *image_path)
{
	boot_img_hdr *hdr;
	char path[32];
	int image_fd, ret, vm_fd, vmid;

	/* read the image to get the entry and other args */
	image_fd = open(image_path, O_RDWR | O_NONBLOCK);
	if (image_fd < 0) {
		perror(image_path);
		return -ENOENT;
	}

	hdr = (boot_img_hdr *)malloc(sizeof(boot_img_hdr));
	if (!hdr)
		return -ENOMEM;

	ret = read_bootimage_header(image_fd, hdr);
	if (ret) {
		printf("%s is not a vaild bootimage\n", image_path);
		goto free_hdr;
	}

	info->entry = hdr->kernel_addr;
	info->setup_data = hdr->second_addr;
	info->mem_start = info->entry & ~(0x200000 - 1);

	vmid = create_new_vm(info);
	if (vmid <= 0)
		goto close_fd;

	memset(path, 0, 32);
	sprintf(path, "/dev/mvm/mvm%d", vmid);
	vm_fd = open(path, O_RDWR | O_NONBLOCK);
	if (vm_fd < 0) {
		perror(path);
		ret =-ENOENT;
		goto destory_vm;
	}

	ret = load_boot_image(vm_fd, image_fd, info, hdr);
	if (ret)
		goto destory_vm;

	/*
	 * do other setup things
	 */
	setup_vm_dts(vm_fd, info, hdr);

	while (1) {

	}

destory_vm:
	destory_vm(vmid);
close_fd:
	close(image_fd);
free_hdr:
	free(hdr);
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

	if ((buf[len] = 'm') && (buf[len] != 'M'))
		return -EINVAL;

	buf[len] = '\0';
	*size = atol(buf);

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

int main(int argc, char **argv)
{
	int ret, opt, idx;
	struct vm_info *info;
	char *image_path;
	char *optstr = "c:m:i:s:n:t:b:?h";

	info = (struct vm_info *)malloc(sizeof(struct vm_info));
	if (!info)
		return -ENOMEM;

	image_path = malloc(256);
	if (!image_path) {
		free(info);
		return -ENOMEM;
	}

	memset(&info, 0, sizeof(struct vm_info));
	while ((opt = getopt_long(argc, argv, optstr, options, &idx)) != -1) {
		switch(opt) {
		case 'c':
			info->nr_vcpus = atoi(optarg);
			break;
		case 'm':
			ret = parse_vm_memsize(optarg, &info->mem_size);
			if (ret)
				print_usage();
			break;
		case 'i':
			strcpy(image_path, optarg);
			break;
		case 's':
			ret = parse_vm_membase(optarg, (uint64_t *)&info->mem_start);
			if (ret)
				print_usage();
			break;
		case 'n':
			strncpy((char *)info->name, optarg, 31);
			break;
		case 't':
			strncpy((char *)info->os_type, optarg, 31);
			break;
		case 'b':
			ret = atoi(optarg);
			if ((ret != 32) && (ret != 64))
				print_usage();
			info->bit64 = ret == 64 ? 1 : 0;
			break;
		case 'r':
			no_ramdisk = 1;
			break;
		case 'h':
			print_usage();
			break;
		}
	}

	if (info->nr_vcpus > VM_MAX_VCPUS)
		info->nr_vcpus = VM_MAX_VCPUS;

	ret = main_loop(info, image_path);
	if (ret) {
		perror("run vm failed\n");
		free(info);
		free(image_path);
		return ret;
	}

	return 0;
}
