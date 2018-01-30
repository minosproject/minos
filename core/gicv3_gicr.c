#include <core/types.h>
#include <core/io.h>
#include <asm/gicv3.h>
#include <core/spinlock.h>
#include <core/percpu.h>

static unsigned long __gicr_rd_base = 0x2f100000;

DEFINE_PER_CPU(unsigned long, gicr_rd_base);
DEFINE_PER_CPU(unsigned long, gicr_sgi_base);

#define gicr_rd_base()	get_cpu_var(gicr_rd_base)
#define gicr_sgi_base()	get_cpu_var(gicr_sgi_base)

void wakeup_gicr(void)
{
	uint32_t gic_waker_value;

	gic_waker_value = ioread32(gicr_rd_base() + GICR_WAKER);
	gic_waker_value &= ~(GICR_WAKER_PROCESSOR_SLEEP);
	iowrite32(gicr_rd_base() + GICR_WAKER, gic_waker_value);

	while ((ioread32(gicr_rd_base() + GICR_WAKER)
			& GICR_WAKER_CHILDREN_ASLEEP) != 0);
}

void enable_private_int(uint32_t id)
{
	iowrite32(gicr_sgi_base() + GICR_ISENABLER, 1 << (id & 0x1f));
}

void set_private_int_priority(uint32_t id, uint32_t priority)
{
	uint32_t base = GICR_IPRIORITYR_OFFSET(id);

	iowrite8(gicr_sgi_base() + base, priority);
}

uint32_t get_private_int_priority(uint32_t id)
{
	uint32_t base = GICR_IPRIORITYR_OFFSET(id);

	return (uint32_t)ioread8(gicr_sgi_base() + base);
}

void set_private_int_pending(uint32_t id)
{
	iowrite32(gicr_sgi_base() + GICR_ISPENDR, 1 << (id & 0x1f));
}

uint32_t get_private_int_pending(uint32_t id)
{
	return (ioread32(gicr_sgi_base() + GICR_ISPENDR) >> id) & 0x1f;
}

void clear_private_int_pending(uint32_t id)
{
	iowrite32(gicr_sgi_base() + GICR_ICPENDR, (1 << (id & 0x1f)));
}

void set_private_int_sec(uint32_t id, int group)
{
	uint32_t groupmod;
	uint32_t g0_value;
	uint32_t g0_mode_value;

	id &= 0x1f;
	groupmod = (group >> 1) & 1;
	group &= 1;

	g0_value = ioread32(gicr_sgi_base() + GICR_IGROUPR0);
	g0_mode_value = ioread32(gicr_sgi_base() + GICR_IGRPMODR0);

	if (group) {
		iowrite32(gicr_sgi_base() + GICR_IGROUPR0,
				(g0_value | (1 << id)));
	} else {
		iowrite32(gicr_sgi_base() + GICR_IGROUPR0,
				(g0_value & ~(1 << id)));
	}

	if (groupmod) {
		iowrite32(gicr_sgi_base() + GICR_IGRPMODR0,
				(g0_mode_value) | (1 << id));
	} else {
		iowrite32(gicr_sgi_base() + GICR_IGRPMODR0,
				(g0_mode_value) & ~(1 << id));
	}
}

void set_private_int_sec_block(uint32_t group)
{
	uint32_t nbits = (sizeof(uint32_t) * 8) -1;
	uint32_t groupmod;

	groupmod = (uint32_t)(((int32_t)group << (nbits - 1)) >> 31);
	group = (uint32_t)(((int32_t)group << nbits) >> 31);

	iowrite32(gicr_sgi_base() + GICR_IGROUPR0, group);
	iowrite32(gicr_sgi_base() + GICR_IGRPMODR0, groupmod);
}

int gic_gicr_global_init(void)
{
	int i;
	unsigned long rbase;

	for (i = 0; i < CONFIG_NUM_OF_CPUS; i++) {
		rbase = __gicr_rd_base + (128 * 1024) * i;
		get_per_cpu(gicr_rd_base, i) = rbase;
		get_per_cpu(gicr_sgi_base, i) = rbase + (64 * 1024);
	}
}
