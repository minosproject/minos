/*
 * created by Le MIn 2017/12/09
 */

#include <core/vm.h>
#include <asm/cpu.h>

static int os1_boot_vm(uint64_t ram_base, uint64_t ram_size,
		struct vmm_vcpu_context *c, uint32_t vcpu_id)
{
	c->x0 = 0;
	c->x1 = 1;
	c->x2 = 2;
	c->x3 = 3;
	c->x4 = 4;
	c->x5 = 5;
	c->x6 = 6;
	c->x7 = 7;
	c->x8 = 8;
	c->x9 = 9;
	c->x10 = 10;
	c->x11 = 11;
	c->x12 = 12;
	c->x13 = 13;
	c->x14 = 14;
	c->x15 = 15;
	c->x16 = 16;
	c->x17 = 17;
	c->x18 = 18;
	c->x19 = 19;
	c->x20 = 20;
	c->x21 = 21;
	c->x22 = 22;
	c->x23 = 23;
	c->x24 = 24;
	c->x25 = 25;
	c->x26 = 26;
	c->x27 = 27;
	c->x28 = 28;
	c->x29 = 29;
	c->x30_lr = 30;
	c->sp_el1 = 0x0;
	c->elr_el2 = 0x90010000;
	c->vbar_el1 = 0;
	c->spsr_el1 = AARCH64_SPSR_EL1h | AARCH64_SPSR_F | \
		      AARCH64_SPSR_I | AARCH64_SPSR_A;
	c->nzcv = 0;
	c->esr_el1 = 0x0;
}

struct vmm_vm_entry vm_os1 = {
	.name = "os1",
	.ram_base = 0x9000000,
	.ram_size = (256 * 1024 *1024),
	.nr_vcpu = 4,
	.vcpu_affinity = {0},
	.boot_vm = os1_boot_vm,
};

register_vm_entry(vm_os1);
