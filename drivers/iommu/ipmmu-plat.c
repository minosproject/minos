// SPDX-License-Identifier: GPL-2.0

/*
 * Based on Renesas R-Car System Controller driver (rcar-sysc).
 */

#include <asm/io.h>
#include <minos/errno.h>
#include <minos/mmu.h>
#include <minos/of.h>

static void *rcar_sysc_base = NULL;

/* SYSC MMIO range */
#define RCAR_SYSC_BASE		0xe6180000
#define RCAR_SYSC_SIZE		0x400

/*
 * These power domain indices match the numbers of the interrupt bits
 * representing the power areas in the various Interrupt Registers
 * (e.g. SYSCISR, Interrupt Status Register)
 */
#define RCAR_GEN3_PD_A3VP	9
#define RCAR_GEN3_PD_A3VC	14
/* Always-on power area */
#define RCAR_GEN3_PD_ALWAYS_ON	32

/* SYSC Common */
#define SYSCSR			0x00	/* SYSC Status Register */
#define SYSCISR			0x04	/* Interrupt Status Register */
#define SYSCISCR		0x08	/* Interrupt Status Clear Register */
#define SYSCIER			0x0c	/* Interrupt Enable Register */
#define SYSCIMR			0x10	/* Interrupt Mask Register */

/* SYSC Status Register */
#define SYSCSR_PONENB		1	/* Ready for power resume requests */

/* Power Control Register Offsets inside the register block for each domain */
#define PWRSR_OFFS		0x00	/* Power Status Register */
#define PWRONCR_OFFS		0x0c	/* Power Resume Control Register */
#define PWRER_OFFS		0x14	/* Power Shutoff/Resume Error */

#define SYSCSR_RETRIES		1000
#define SYSCSR_DELAY_US		10

#define PWRER_RETRIES		1000
#define PWRER_DELAY_US		10

#define SYSCISR_RETRIES		1000
#define SYSCISR_DELAY_US	10

struct rcar_sysc_ch {
	const char *name;
	u16 chan_offs;			/* Offset of PWRSR register for this area */
	u8 chan_bit;			/* Bit in PWR* (except for PWRUP in PWRSR) */
	u8 isr_bit;			/* Bit in SYSCI*R */
};

/*
 * For the most of IPMMU-XX which are located in ALWAYS_ON power domain
 * we don't care at all. But some of them are located in other domains
 * and must be turned on once at boot.
 * Hopefully, the each of domains we are dealing with within this file
 * (A3VP, A3VP) is identically configured across all SoCs (H3, M3 and M3N).
 * This allow us not to introduce support for each SoC separately.
 */
static const struct rcar_sysc_ch rcar_sysc_chs[2] = {
	{
		.name = "A3VP",
		.chan_offs = 0x340,
		.chan_bit = 0,
		.isr_bit = RCAR_GEN3_PD_A3VP,
	},
	{
		.name = "A3VC",
		.chan_offs = 0x380,
		.chan_bit = 0,
		.isr_bit = RCAR_GEN3_PD_A3VC,
	},
};

static int rcar_sysc_init(void)
{
	u32 syscier, syscimr;
	int i;

	/*
	 * As this function might be called more then once, just return if we
	 * have already initialized sysc.
	 */
	if (rcar_sysc_base)
		return 0;

	rcar_sysc_base = (void *)io_remap(RCAR_SYSC_BASE, RCAR_SYSC_SIZE);
	if (!rcar_sysc_base) {
		pr_err("failed to map SYSC MMIO range\n");
		return -ENOMEM;
	}

	syscier = 0;
	for (i = 0; i < ARRAY_SIZE(rcar_sysc_chs); i++)
		syscier |= BIT(rcar_sysc_chs[i].isr_bit);

	/*
	 * Mask all interrupt sources to prevent the CPU from receiving them.
	 * Make sure not to clear reserved bits that were set before.
	 */
	syscimr = readl(rcar_sysc_base + SYSCIMR);
	syscimr |= syscier;
	writel(syscimr, rcar_sysc_base + SYSCIMR);

	/* SYSC needs all interrupt sources enabled to control power */
	writel(syscier, rcar_sysc_base + SYSCIER);

	return 0;
}

static bool rcar_sysc_power_is_off(const struct rcar_sysc_ch *sysc_ch)
{
	unsigned int status;

	status = readl(rcar_sysc_base + sysc_ch->chan_offs + PWRSR_OFFS);
	if (status & BIT(sysc_ch->chan_bit))
		return true;

	return false;
}

static int rcar_sysc_power_on(const struct rcar_sysc_ch *sysc_ch)
{
	unsigned int status;
	int ret = 0, i, j;

	writel(BIT(sysc_ch->isr_bit), rcar_sysc_base + SYSCISCR);

	/* Submit power resume request until it was accepted */
	for (i = 0; i < PWRER_RETRIES; i++) {

		/* Wait until SYSC is ready to accept a power request */
		for (j = 0; j < SYSCSR_RETRIES; j++) {
			if (readl(rcar_sysc_base + SYSCSR) & BIT(SYSCSR_PONENB))
				break;
			udelay(SYSCSR_DELAY_US);
		}

		if (j == SYSCSR_RETRIES)
			return -EAGAIN;

		/* Submit power resume request */
		writel(BIT(sysc_ch->chan_bit),
		       rcar_sysc_base + sysc_ch->chan_offs + PWRONCR_OFFS);

		status = readl(rcar_sysc_base + sysc_ch->chan_offs + PWRER_OFFS);
		if (!(status & BIT(sysc_ch->chan_bit)))
			break;
		udelay(PWRER_DELAY_US);
	}

	if (i == PWRER_RETRIES)
		return -EIO;

	/* Wait until the power resume request has completed */
	for (i = 0; i < SYSCISR_RETRIES; i++) {
		if (readl(rcar_sysc_base + SYSCISR) & BIT(sysc_ch->isr_bit))
			break;
		udelay(SYSCISR_DELAY_US);
	}

	if (i == SYSCISR_RETRIES)
		ret = -EIO;

	writel(BIT(sysc_ch->isr_bit), rcar_sysc_base + SYSCISCR);

	return ret;
}

static uint32_t ipmmu_get_mmu_pd(struct device_node *node)
{
	uint32_t value[2];
	int ret;

	ret = of_get_u32_array(node, "power-domains", value, ARRAY_SIZE(value));
	if (ret <= 0)
		return -ENODEV;

	return value[1];
}

/*
 * Some IPMMU-XX are not located in ALWAYS_ON power domain
 * (IPMMU-VP0, IPMMU-VC0 belong to A3xx power domains) and as the result
 * they are in power-off state during booting, therefore they must be
 * explicitly powered on before initializing.
 */
int ipmmu_power_on(struct device_node *node)
{
	int i, pd, ret = -ENODEV;

	pd = ipmmu_get_mmu_pd(node);
	if (pd < 0 || pd == RCAR_GEN3_PD_ALWAYS_ON)
		return 0;

	rcar_sysc_init();

	for (i = 0; i < ARRAY_SIZE(rcar_sysc_chs); i++) {
		if (rcar_sysc_chs[i].isr_bit != pd)
			continue;

		if (!rcar_sysc_power_is_off(&rcar_sysc_chs[i])) {
			pr_notice("%s: %s domain is already powered on\n",
				  devnode_name(node), rcar_sysc_chs[i].name);
			return 0;
		}

		ret = rcar_sysc_power_on(&rcar_sysc_chs[i]);
		if (ret) {
			pr_err("%s: failed to power on %s domain\n",
			       devnode_name(node), rcar_sysc_chs[i].name);
			break;
		}

		pr_notice("%s: powered on %s domain\n", devnode_name(node),
			  rcar_sysc_chs[i].name);
		return 0;
	}

	return ret;
}

/*
 * Check if we will have to disable IPMMU TLB cache function of IPMMU caches
 * that belong to non ALWAYS_ON power domain (IPMMU-VP0, IPMMU-VC0 belong
 * to A3xx power domains) due to H/W restriction.
 * Required action will be performed right before enabling corresponding
 * IPMMU-XX.
 */
bool ipmmu_is_mmu_tlb_disable_needed(struct device_node *node)
{
	int i, pd;

	pd = ipmmu_get_mmu_pd(node);
	if (pd < 0 || pd == RCAR_GEN3_PD_ALWAYS_ON)
		return false;

	/* Actually check among power domains we have already powered on */
	for (i = 0; i < ARRAY_SIZE(rcar_sysc_chs); i++) {
		if (rcar_sysc_chs[i].isr_bit == pd)
			return true;
	}

	return false;
}
