/*
 * Copyright (C) 2018 Min Le (lemin9538@gmail.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <minos/minos.h>
#include <asm/arch.h>
#include <asm/io.h>
#include <minos/mmu.h>
#include <asm/bcm_irq.h>
#include <libfdt/libfdt.h>
#include <minos/of.h>
#include <asm/cpu.h>
#include <minos/platform.h>

#ifdef CONFIG_VIRT
#include <virt/virq.h>
#include <virt/vm.h>
#include <virt/vmm.h>
#include <virt/vdev.h>

#define BCM2838_RELEASE_ADDR	(0xff800000)

static int bcm2838_fake_scu_read(struct vdev *vdev, gp_regs *regs,
		unsigned long address, unsigned long *value)
{
	return 0;
}

static int bcm2838_fake_scu_write(struct vdev *vdev, gp_regs *regs,
		unsigned long address, unsigned long *value)
{
	int cpu;
	struct vm *vm = vdev->vm;
	unsigned long offset = address - BCM2838_RELEASE_ADDR;

	if (offset % sizeof(uint64_t)) {
		pr_err("unsupport address 0x%x\n", address);
		return -EINVAL;
	}

	cpu = offset / sizeof(uint64_t);
	if (cpu >= vm->vcpu_nr) {
		pr_err("no such vcpu vcpu-id:%d\n", cpu);
		return -EINVAL;
	}

	vcpu_power_on(get_current_vcpu(), cpuid_to_affinity(cpu),
			*value, 0);

	return 0;
}

static int raspberry4_setup_hvm(struct vm *vm, void *dtb)
{
	int i, offset, node;
	char name[16];
	uint64_t addr;
	uint64_t dtb_addr = 0;
	uint32_t *tmp = (uint32_t *)&dtb_addr;
	struct vdev *vdev;

	offset = of_get_node_by_name(dtb, 0, "cpus");
	if (offset < 0) {
		pr_err("can not find vcpus node for hvm\n");
		return -ENOENT;
	}

	/*
	 * using spin table boot methold redirect the
	 * relase addr to the interrupt controller space
	 */
	for (i = 0; i < vm->vcpu_nr; i++) {
		memset(name, 0, 16);
		sprintf(name, "cpu@%d", i);
		node = fdt_subnode_offset(dtb, offset, name);
		if (node <= 0)
			continue;

		addr = BCM2838_RELEASE_ADDR + i * sizeof(uint64_t);
		pr_info("vcpu-%d release addr redirect to 0x%p\n", i, addr);
		tmp[0] = cpu_to_fdt32(addr >> 32);
		tmp[1] = cpu_to_fdt32(addr & 0xffffffff);

		fdt_setprop(dtb, node, "cpu-release-addr", (void *)tmp,
				2 * sizeof(uint32_t));
	}

	/* register a fake system controller for smp up handler */
	if (vm->vcpu_nr > 1) {
		vdev = zalloc(sizeof(struct vdev));
		if (!vdev)
			panic("no more memory for spi-table\n");

		host_vdev_init(vm, vdev, BCM2838_RELEASE_ADDR, 0x1000);
		vdev_set_name(vdev, "fake-controller");

		vdev->read = bcm2838_fake_scu_read;
		vdev->write = bcm2838_fake_scu_write;
	}

	pr_info("raspberry4 setup vm done\n");

	return 0;
}
#endif

static void raspberry4_system_reboot(int mode, const char *cmd)
{

}

static void raspberry4_system_shutdown(void)
{

}

static void raspberry4_parse_mem_info(void)
{

}

static struct platform platform_raspberry4 = {
	.name 		 = "raspberrypi,4-model-b",
	.cpu_on		 = spin_table_cpu_on,
	.system_reboot	 = raspberry4_system_reboot,
	.system_shutdown = raspberry4_system_shutdown,
#ifdef CONFIG_VIRT
	.setup_hvm	 = raspberry4_setup_hvm,
#endif
	.parse_mem_info  = raspberry4_parse_mem_info,
};
DEFINE_PLATFORM(platform_raspberry4);
