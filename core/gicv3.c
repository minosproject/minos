#include <core/types.h>
#include <core/io.h>
#include <asm/gicv3.h>
#include <core/spinlock.h>
#include <core/percpu.h>

static unsigned long gicd_base = 0x2f000000;
static unsigned long __gicr_rd_base = 0x2f100000;

DEFINE_PER_CPU(unsigned long, gicr_rd_base);
DEFINE_PER_CPU(unsigned long, gicr_sgi_base);

spinlock_t gicd_lock;

#define gicr_rd_base()	get_cpu_var(gicr_rd_base)
#define gicr_sgi_base()	get_cpu_var(gicr_sgi_base)

void config_gicd(uint32_t value)
{
	spin_lock(&gicd_lock);
	iowrite32(gicd_base + GICD_CTLR, value);
	spin_unlock(&gicd_lock);
}

void enable_gicd(uint32_t flags)
{
	uint32_t value;

	spin_lock(&gicd_lock);
	value = ioread32(gicd_base + GICD_CTLR);
	iowrite32(gicd_base + GICD_CTLR, value | flags);
	spin_unlock(&gicd_lock);
}

void disable_gicd(uint32_t flags)
{
	uint32_t value;

	spin_lock(&gicd_lock);
	ioread32(gicd_base + GICD_CTLR);
	iowrite32(gicd_base + GICD_CTLR, value & ~(flags));
	spin_unlock(&gicd_lock);
}

void sync_are_in_gicd(int flags, uint32_t dosync)
{
	if (dosync) {
		/*
		 * other cpu check whether ARE is ok
		 */
		uint32_t tmask = GICD_CTLR_ARE_S | GICD_ARE_NS;
		uint32_t tval = flags & tmask;

		while ((ioread32(gicd_base + GICD_CTLR) & tmask) != tval);

	} else {
		/*
		 * boot cpu enable the ARE
		 */
		spin_lock(&gicd_lock);
		iowrite32(gicd_base + GICD_CTLR, flags);
		spin_unlock(&gicd_lock);
	}
}

void enable_spi(uint32_t id)
{
	spin_lock(&gicd_lock);
	iowrite32(gicd_base + GICD_ISENABLER_OFFSET(id),
			1 << (id & 0x1f));
	spin_unlock(&gicd_lock);
}

void disable_spi(uint32_t id)
{
	spin_lock(&gicd_lock);
	iowrite32(gicd_base + GICD_ICENABLER_OFFSET(id),
			1 << (id & 0x1f));
	spin_unlock(&gicd_lock);
}

void set_spi_int_priority(uint32_t id, uint32_t priority)
{
	spin_lock(&gicd_lock);
	iowrite8(gicd_base + GICD_IPRIORITYR_OFFSET(id), priority);
	spin_unlock(&gicd_lock);
}

uint32_t get_spi_int_priority(uint32_t id)
{
	return (uint32_t)ioread8(gicd_base + GICD_IPRIORITYR_OFFSET(id));
}

void set_spi_int_route(uint32_t id, uint32_t affinity, uint64_t mode)
{
	spin_lock(&gicd_lock);
	iowrite64(gicd_base + GICD_IROUTER_OFFSET(id), affinity | mode);
	spin_unlock(&gicd_lock);
}

void set_spi_int_target(uint32_t id, uint32_t target)
{
	spin_lock(&gicd_lock);
	iowrite8(gicd_base + GICD_ITARGETSR_OFFSET(id), target);
	spin_unlock(&gicd_lock);
}

uint32_t get_spi_int_target(uint32_t id)
{
	return (uint32_t)ioread8(gicd_base + GICD_ITARGETSR_OFFSET(id));
}

void config_spi_int(uint32_t id, uint32_t config)
{
	uint32_t tmp;

	config &= 3;
	id = (id & 0xf) << 1;

	tmp = ioread32(gicd_base + GICD_ICFGR_OFFSET(id));
	tmp &= ~(3 << id);
	tmp |= config << id;

	spin_lock(&gicd_lock);
	iowrite32(gicd_base + GICD_ICFGR_OFFSET(id), tmp);
	spin_unlock(&gicd_lock);
}

void set_spi_int_pending(uint32_t id)
{
	iowrite32(gicd_base + GICD_ISPENDR_OFFSET(id), 1 << (1 & 0x1f));
}

void clear_spi_int_pending(uint32_t id)
{
	spin_lock(&gicd_lock);
	iowrite32(gicd_base + GICD_ICPENDR_OFFSET(id), 1 << (id & 0x1f));
	spin_unlock(&gicd_lock);
}

uint32_t get_spi_int_pending(uint32_t id)
{
	return (ioread32(gicd_base + GICD_ICPENDR_OFFSET(id)) >> id) & 1;
}

void set_spi_int_sec(uint32_t id, int group)
{
	uint32_t groupmod;
	uint32_t value;

	id &= 0x1f;
	groupmod = (group >> 1) & 1;
	group &= 1;

	spin_lock(&gicd_lock);
	value = ioread32(gicd_base + GICD_IGROUPR_OFFSET(id));
	if (group)
		iowrite32(gicd_base + GICD_IGROUPR_OFFSET(id), value | (1 << id));
	else
		iowrite32(gicd_base + GICD_IGROUPR_OFFSET(id), value & ~(1 << id));

	value = ioread32(gicd_base + GICD_IGRPMODR_OFFSET(id));
	if (groupmod)
		iowrite32(gicd_base + GICD_IGRPMODR_OFFSET(id), value | (1 << id));
	else
		iowrite32(gicd_base + GICD_IGRPMODR_OFFSET(id), value & ~(1 << id));
	spin_unlock(&gicd_lock);
}

void set_spi_int_sec_block(uint32_t block, uint32_t group)
{
	uint32_t groupmod;
	uint32_t nbits = (sizeof(uint32_t) * 8) - 1;

	block &= 31;

	groupmod = (uint32_t)(((int32_t)group << (nbits - 1)) >> 31);
	group = (uint32_t)(((int32_t)group << nbits) >> 31);

	spin_lock(&gicd_lock);
	iowrite32(gicd_base + GICD_IGROUPR_OFFSET(block), group);
	iowrite32(gicd_base + GICD_IGRPMODR_OFFSET(block), groupmod);
	spin_unlock(&gicd_lock);
}

void set_spi_int_sec_all(uint32_t group)
{
	uint32_t block, value;

	value = ioread32(gicd_base + GICD_TYPER);
	value = value & ((1 << 5) - 1);

	for (block = value; block > 0; --block)
		set_spi_int_sec_block(block, group);
}

uint64_t get_spi_route(uint32_t id)
{
	return (uint64_t)ioread64(gicd_base + GICD_IROUTER_OFFSET(id));
}

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
	iowrite32(gicr_sgi_base() + GICR_ICENABLER, 1 << (id & 0x1f));
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

void set_private_int_sec_block(int group)
{
	uint32_t nbits = (sizeof(uint32_t) * 8) -1;
	uint32_t groupmod;

	groupmod = (uint32_t)(((int32_t)group << (nbits - 1)) >> 31);
	group = (uint32_t)(((int32_t)group << nbits) >> 31);

	iowrite32(gicr_sgi_base() + GICR_IGROUPR0, group);
	iowrite32(gicr_sgi_base() + GICR_IGRPMODR0, groupmod);
}

void gic_global_init(void)
{
	int i;
	unsigned long rbase;

	for (i = 0; i < CONFIG_NUM_OF_CPUS; i++) {
		rbase = __gicr_rd_base + (128 * 1024) * i;
		get_per_cpu(gicr_rd_base, i) = rbase;
		get_per_cpu(gicr_sgi_base, i) = rbase + (64 * 1024);
	}

	spin_lock_init(&gicd_lock);
}
