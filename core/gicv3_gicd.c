#include <core/types.h>
#include <core/io.h>
#include <asm/gicv3.h>
#include <core/spinlock.h>
#include <core/percpu.h>

static unsigned long gicd_base = 0x2f000000;
spinlock_t gicd_lock;

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
		uint32_t tmask = GICD_CTLR_ARE_NS;
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

int gic_gicd_global_init(void)
{
	spin_lock_init(&gicd_lock);
	return 0;
}
