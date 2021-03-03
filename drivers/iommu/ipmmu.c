// SPDX-License-Identifier: GPL-2.0

#include <asm/io.h>
#include <minos/irq.h>
#include <virt/vm.h>
#include <minos/of.h>
#include <virt/iommu.h>

extern int ipmmu_power_on(struct device_node *node);
extern bool ipmmu_is_mmu_tlb_disable_needed(struct device_node *node);

/*
 * Start of Minos specific code
 */

/* Minos: Dummy iommu_domain */
struct iommu_domain {
	atomic_t ref;
	/*
	 * Used to link iommu_domain contexts for a same VM.
	 * There is at least one per-IPMMU to used by the VM.
	 */
	struct list_head list;
};

/* Minos: Describes informations required for a Minos VM */
struct vm_ipmmu {
	spinlock_t lock;
	/* List of context (i.e iommu_domain) associated to this VM */
	struct list_head contexts;
	struct iommu_domain *base_context;
};

#define to_vm_ipmmu(vm) ((struct vm_ipmmu *)(vm)->iommu.priv)
#define to_node_ipmmu(node) ((struct node_ipmmu *)(node)->iommu.priv)

/*
 * Start of Linux IPMMU code
 */

#define IPMMU_CTX_MAX 8

#define IPMMU_PER_DEV_MAX 4

struct ipmmu_device {
	struct device_node *node;
	void *base;
	struct list_head list;
	bool is_leaf;
	unsigned int num_utlbs;
	unsigned int num_ctx;
	spinlock_t lock;			/* Protects ctx and domains[] */
	DECLARE_BITMAP(ctx, IPMMU_CTX_MAX);
	struct ipmmu_domain *domains[IPMMU_CTX_MAX];
};

struct ipmmu_domain {
	/* Cache IPMMUs the master device can be tied to */
	struct ipmmu_device *mmus[IPMMU_PER_DEV_MAX];
	unsigned int num_mmus;
	struct ipmmu_device *root;
	struct iommu_domain io_domain;

	unsigned int context_id;
	spinlock_t lock;			/* Protects mappings */

	/* Minos: VM associated to this configuration */
	struct vm *vm;
};

struct ipmmu_utlb {
	/* Cache IPMMU the uTLB is connected to */
	struct ipmmu_device *mmu;
	unsigned int utlb;
};

/*
 * Minos: Information about each device stored in node->iommu.priv
 *
 * On Linux the dev->archdata.iommu only stores the arch specific information,
 * but, on Minos, we also have to store the iommu domain.
 */
struct node_ipmmu {
	struct iommu_domain *io_domain;

	/* Cache IPMMUs the master device can be tied to */
	struct ipmmu_device *mmus[IPMMU_PER_DEV_MAX];
	unsigned int num_mmus;
	struct ipmmu_utlb *utlbs;
	unsigned int num_utlbs;
	struct device_node *node;
	struct list_head list;
};

static DEFINE_SPIN_LOCK(ipmmu_devices_lock);
static LIST_HEAD(ipmmu_devices);

#define to_ipmmu_domain(io_dom)                                                \
	(container_of((io_dom), struct ipmmu_domain, io_domain))

#define TLB_LOOP_TIMEOUT		100	/* 100us */

/*
 * Registers Definition
 */

#define IM_CTX_SIZE			0x40

#define IMCTR				0x0000
#define IMCTR_VA64			(1 << 29)
#define IMCTR_INTEN			(1 << 2)
#define IMCTR_FLUSH			(1 << 1)
#define IMCTR_MMUEN			(1 << 0)

#define IMTTBCR				0x0008
#define IMTTBCR_EAE			(1 << 31)
#define IMTTBCR_PMB			(1 << 30)
#define IMTTBCR_SH0_INNER_SHAREABLE	(3 << 12)
#define IMTTBCR_ORGN0_WB_WA		(1 << 10)
#define IMTTBCR_IRGN0_WB_WA		(1 << 8)
#define IMTTBCR_TSZ0_SHIFT		0

#define IMTTBCR_SL0_TWOBIT_LVL_1	(2 << 6)

#define IMTTLBR0			0x0010
#define IMTTUBR0			0x0014

#define IMTTLBR_MASK			0xFFFFF000

#define IMSTR				0x0020
#define IMSTR_MHIT			(1 << 4)
#define IMSTR_ABORT			(1 << 2)
#define IMSTR_PF			(1 << 1)
#define IMSTR_TF			(1 << 0)

#define IMEAR				0x0030
#define IMEUAR				0x0034

#define IMUCTR(n)			((n) < 32 ? IMUCTR0(n) : IMUCTR32(n))
#define IMUCTR0(n)			(0x0300 + ((n) * 16))
#define IMUCTR32(n)			(0x0600 + (((n) - 32) * 16))
#define IMUCTR_TTSEL_MMU(n)		((n) << 4)
#define IMUCTR_FLUSH			(1 << 1)
#define IMUCTR_MMUEN			(1 << 0)

#define IMUASID(n)			((n) < 32 ? IMUASID0(n) : IMUASID32(n))
#define IMUASID0(n)			(0x0308 + ((n) * 16))
#define IMUASID32(n)			(0x0608 + (((n) - 32) * 16))

#define IMSCTLR				0x0500
#define IMSCTLR_DISCACHE		0xE0000000

#define IMSAUXCTLR			0x0504
#define IMSAUXCTLR_S2PTE		(1 << 3)

/*
 * Root device handling
 */

static bool ipmmu_is_root(struct ipmmu_device *mmu)
{
	/* Minos: Fix */
	return mmu && !mmu->is_leaf;
}

static struct ipmmu_device *ipmmu_find_root(struct ipmmu_device *leaf)
{
	struct ipmmu_device *mmu = NULL;

	if (ipmmu_is_root(leaf))
		return leaf;

	spin_lock(&ipmmu_devices_lock);

	list_for_each_entry (mmu, &ipmmu_devices, list) {
		if (ipmmu_is_root(mmu))
			break;
	}

	spin_unlock(&ipmmu_devices_lock);
	return mmu;
}

/*
 * Read/Write Access
 */

static u32 ipmmu_read(struct ipmmu_device *mmu, unsigned int offset)
{
	return ioread32(mmu->base + offset);
}

static void ipmmu_write(struct ipmmu_device *mmu, unsigned int offset, u32 data)
{
	iowrite32(data, mmu->base + offset);
}

static u32 ipmmu_ctx_read_root(struct ipmmu_domain *domain, unsigned int reg)
{
	return ipmmu_read(domain->root, domain->context_id * IM_CTX_SIZE + reg);
}

static void ipmmu_ctx_write_root(struct ipmmu_domain *domain, unsigned int reg,
				 u32 data)
{
	ipmmu_write(domain->root, domain->context_id * IM_CTX_SIZE + reg, data);
}

/* Minos: Write the context for cache IPMMU only. */
static void ipmmu_ctx_write_cache(struct ipmmu_domain *domain, unsigned int reg,
				  u32 data)
{
	unsigned int i;

	for (i = 0; i < domain->num_mmus; i++)
		ipmmu_write(domain->mmus[i],
			    domain->context_id * IM_CTX_SIZE + reg, data);
}

/*
 * Minos: Write the context for both root IPMMU and all cache IPMMUs
 * that assigned to this Minos VM.
 */
static void ipmmu_ctx_write_all(struct ipmmu_domain *domain, unsigned int reg,
				u32 data)
{
	struct vm_ipmmu *vm_ipmmu = to_vm_ipmmu(domain->vm);
	struct iommu_domain *io_domain;

	list_for_each_entry (io_domain, &vm_ipmmu->contexts, list)
		ipmmu_ctx_write_cache(to_ipmmu_domain(io_domain), reg, data);

	ipmmu_ctx_write_root(domain, reg, data);
}

/*
 * TLB and microTLB Management
 */

/* Wait for any pending TLB invalidations to complete */
static void ipmmu_tlb_sync(struct ipmmu_domain *domain)
{
	unsigned int count = 0;

	while (ipmmu_ctx_read_root(domain, IMCTR) & IMCTR_FLUSH) {
		cpu_relax();
		if (++count == TLB_LOOP_TIMEOUT) {
			pr_err("%s: TLB sync timed out -- MMU may be deadlocked\n",
			       devnode_name(domain->root->node));
			return;
		}
		udelay(1);
	}
}

static void ipmmu_tlb_invalidate(struct ipmmu_domain *domain)
{
	u32 reg;

	reg = ipmmu_ctx_read_root(domain, IMCTR);
	reg |= IMCTR_FLUSH;
	ipmmu_ctx_write_all(domain, IMCTR, reg);

	ipmmu_tlb_sync(domain);
}

/* Enable MMU translation for the microTLB. */
static void ipmmu_utlb_enable(struct ipmmu_domain *domain,
			      struct ipmmu_utlb *utlb_p)
{
	struct ipmmu_device *mmu = utlb_p->mmu;
	unsigned int utlb = utlb_p->utlb;

	/*
	 * TODO: Reference-count the microTLB as several bus masters can be
	 * connected to the same microTLB.
	 */

	/* TODO: What should we set the ASID to ? */
	ipmmu_write(mmu, IMUASID(utlb), 0);

	/* TODO: Do we need to flush the microTLB ? */
	ipmmu_write(mmu, IMUCTR(utlb),
		    IMUCTR_TTSEL_MMU(domain->context_id) | IMUCTR_FLUSH |
			    IMUCTR_MMUEN);
}

/*
 * Domain/Context Management
 */

static int ipmmu_domain_allocate_context(struct ipmmu_device *mmu,
					 struct ipmmu_domain *domain)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&mmu->lock, flags);

	ret = find_first_zero_bit(mmu->ctx, mmu->num_ctx);
	if (ret != mmu->num_ctx) {
		mmu->domains[ret] = domain;
		set_bit(ret, mmu->ctx);
	} else
		ret = -EBUSY;

	spin_unlock_irqrestore(&mmu->lock, flags);

	return ret;
}

static int ipmmu_domain_init_context(struct ipmmu_domain *domain)
{
	u64 ttbr;
	u32 tmp;
	int ret;

	/* Minos: Initialize context_id with non-existent value */
	domain->context_id = domain->root->num_ctx;

	/* Find an unused context. */
	ret = ipmmu_domain_allocate_context(domain->root, domain);
	if (ret < 0)
		return ret;

	domain->context_id = ret;

	/*
	 * TTBR0
	 * Use P2M table. With IPA size being forced to 40 bit (pa_range = 2)
	 * we get 3-level P2M with two concatenated translation tables
	 * at level 1. Which seems to be an appropriate case for the IPMMU.
	 */
	BUG_ON(!domain->vm);
	ttbr = domain->vm->mm.pgd_base;

	/* Minos: */
	pr_notice("%s: vm%d: Set IPMMU context %u (pgd 0x%x)\n",
		  devnode_name(domain->root->node), vm_id(domain->vm),
		  domain->context_id, ttbr);

	ipmmu_ctx_write_root(domain, IMTTLBR0, ttbr & IMTTLBR_MASK);
	ipmmu_ctx_write_root(domain, IMTTUBR0, ttbr >> 32);

	/*
	 * TTBCR
	 * We use long descriptors with inner-shareable WBWA tables and allocate
	 * the whole 40-bit VA space to TTBR0.
	 * Bypass stage 1 translation.
	 */
	tmp = IMTTBCR_SL0_TWOBIT_LVL_1;

	tmp |= (64ULL - 40ULL) << IMTTBCR_TSZ0_SHIFT;

	ipmmu_ctx_write_root(
		domain, IMTTBCR,
		IMTTBCR_EAE | IMTTBCR_PMB | IMTTBCR_SH0_INNER_SHAREABLE |
			IMTTBCR_ORGN0_WB_WA | IMTTBCR_IRGN0_WB_WA | tmp);

	/*
	 * IMSTR
	 * Clear all interrupt flags.
	 */
	ipmmu_ctx_write_root(domain, IMSTR, ipmmu_ctx_read_root(domain, IMSTR));

	/*
	 * IMCTR
	 * Enable the MMU and interrupt generation. The long-descriptor
	 * translation table format doesn't use TEX remapping. Don't enable AF
	 * software management as we have no use for it. Flush the TLB as
	 * required when modifying the context registers.
	 * Minos: Enable the context for the root IPMMU only.
	 */
	ipmmu_ctx_write_root(domain, IMCTR,
			     IMCTR_VA64 | IMCTR_INTEN | IMCTR_FLUSH |
				     IMCTR_MMUEN);

	return 0;
}

static void ipmmu_domain_free_context(struct ipmmu_device *mmu,
				      unsigned int context_id)
{
	unsigned long flags;

	spin_lock_irqsave(&mmu->lock, flags);

	clear_bit(context_id, mmu->ctx);
	mmu->domains[context_id] = NULL;

	spin_unlock_irqrestore(&mmu->lock, flags);
}

static void ipmmu_domain_destroy_context(struct ipmmu_domain *domain)
{
	/* Minos: Just return if context_id has non-existent value */
	if (!domain->root || domain->context_id >= domain->root->num_ctx)
		return;

	/*
	 * Disable the context. Flush the TLB as required when modifying the
	 * context registers.
	 *
	 * TODO: Is TLB flush really needed ?
	 * Minos: Disable the context for the root IPMMU only.
	 */
	ipmmu_ctx_write_root(domain, IMCTR, IMCTR_FLUSH);
	ipmmu_tlb_sync(domain);

	ipmmu_domain_free_context(domain->root, domain->context_id);

	/* Minos: Initialize context_id with non-existent value */
	domain->context_id = domain->root->num_ctx;
}

/*
 * Fault Handling
 */

/* Minos: Show vmid in every printk */
static int ipmmu_domain_irq(struct ipmmu_domain *domain)
{
	const u32 err_mask = IMSTR_MHIT | IMSTR_ABORT | IMSTR_PF | IMSTR_TF;
	struct ipmmu_device *mmu = domain->root;
	u32 status;
	u64 iova;

	status = ipmmu_ctx_read_root(domain, IMSTR);
	if (!(status & err_mask))
		return -ENOENT;

	iova = ipmmu_ctx_read_root(domain, IMEAR) |
	       ((u64)ipmmu_ctx_read_root(domain, IMEUAR) << 32);

	/*
	 * Clear the error status flags. Unlike traditional interrupt flag
	 * registers that must be cleared by writing 1, this status register
	 * seems to require 0. The error address register must be read before,
	 * otherwise its value will be 0.
	 */
	ipmmu_ctx_write_root(domain, IMSTR, 0);

	/* Log fatal errors. */
	if (status & IMSTR_MHIT)
		pr_err("%s: vm%d: Multiple TLB hits @0x%x\n",
		       devnode_name(mmu->node), vm_id(domain->vm), iova);
	if (status & IMSTR_ABORT)
		pr_err("%s: vm%d: Page Table Walk Abort @0x%x\n",
		       devnode_name(mmu->node), vm_id(domain->vm), iova);

	if (!(status & (IMSTR_PF | IMSTR_TF)))
		return -ENOENT;

	/* Flush the TLB as required when IPMMU translation error occurred. */
	ipmmu_tlb_invalidate(domain);

	/*
	 * Try to handle page faults and translation faults.
	 *
	 * TODO: We need to look up the faulty device based on the I/O VA. Use
	 * the IOMMU device for now.
	 */

	pr_err("%s: vm%d: Unhandled fault: status 0x%x iova 0x%x\n",
	       devnode_name(mmu->node), vm_id(domain->vm), status, iova);

	return 0;
}

static int ipmmu_irq(uint32_t irq, void *dev)
{
	struct ipmmu_device *mmu = dev;
	int status = -ENOENT;
	unsigned int i;
	unsigned long flags;

	spin_lock_irqsave(&mmu->lock, flags);

	/* Check interrupts for all active contexts. */
	for (i = 0; i < mmu->num_ctx; i++) {
		if (!mmu->domains[i])
			continue;
		if (ipmmu_domain_irq(mmu->domains[i]) == 0)
			status = 0;
	}

	spin_unlock_irqrestore(&mmu->lock, flags);

	return status;
}

bool ipmmus_are_equal(struct ipmmu_domain *domain,
		      struct node_ipmmu *node_ipmmu)
{
	unsigned int i;

	if (domain->num_mmus != node_ipmmu->num_mmus)
		return false;

	for (i = 0; i < node_ipmmu->num_mmus; i++) {
		if (domain->mmus[i] != node_ipmmu->mmus[i])
			return false;
	}

	return true;
}

static int ipmmu_attach_node(struct iommu_domain *io_domain,
			     struct device_node *node)
{
	struct node_ipmmu *node_ipmmu = to_node_ipmmu(node);
	struct ipmmu_device *root;
	struct ipmmu_domain *domain = to_ipmmu_domain(io_domain);
	unsigned long flags;
	unsigned int i;
	int ret = 0;

	for (i = 0; i < node_ipmmu->num_mmus; i++) {
		if (!node_ipmmu->mmus[i])
			break;
	}

	if (!node_ipmmu->num_mmus || i != node_ipmmu->num_mmus) {
		pr_err("%s: Cannot attach to IPMMU\n", devnode_name(node));
		return -ENXIO;
	}

	root = ipmmu_find_root(node_ipmmu->mmus[0]);
	if (!root) {
		pr_err("%s: Unable to locate root IPMMU\n", devnode_name(node));
		return -EAGAIN;
	}

	spin_lock_irqsave(&domain->lock, flags);

	if (!domain->mmus[0]) {
		/* The domain hasn't been used yet, initialize it. */
		domain->num_mmus = node_ipmmu->num_mmus;
		memcpy(domain->mmus, node_ipmmu->mmus,
		       node_ipmmu->num_mmus * sizeof(*node_ipmmu->mmus));
		domain->root = root;

		/*
		 * Minos: We have already initialized and enabled context for root IPMMU
		 * for this Minos VM. Enable context for given cache IPMMU only.
		 * Flush the TLB as required when modifying the context registers.
		 */

		ipmmu_ctx_write_cache(domain, IMCTR,
				      ipmmu_ctx_read_root(domain, IMCTR) |
					      IMCTR_FLUSH);

		pr_info("%s: Using IPMMU context %u\n", devnode_name(node),
			domain->context_id);
	} else if (!ipmmus_are_equal(domain, node_ipmmu)) {
		/*
		 * Something is wrong, we can't attach two devices using
		 * different IOMMUs to the same domain.
		 */
		for (i = 0; i < node_ipmmu->num_mmus || i < domain->num_mmus;
		     i++)
			pr_err("%s: Can't attach IPMMU%d %s to domain on IPMMU%d %s\n",
			       devnode_name(node), i + 1,
			       i < node_ipmmu->num_mmus ?
				       devnode_name(node_ipmmu->mmus[i]->node) :
				       "---",
			       i + 1,
			       i < domain->num_mmus ?
				       devnode_name(domain->mmus[i]->node) :
				       "---");
		ret = -EINVAL;
	} else {
		pr_info("%s: Reusing IPMMU context %u\n", devnode_name(node),
			domain->context_id);
	}

	spin_unlock_irqrestore(&domain->lock, flags);

	if (ret < 0)
		return ret;

	for (i = 0; i < node_ipmmu->num_utlbs; ++i)
		ipmmu_utlb_enable(domain, &node_ipmmu->utlbs[i]);

	return 0;
}

static int ipmmu_find_utlbs(struct device_node *node, struct ipmmu_utlb *utlbs,
			    unsigned int num_utlbs)
{
	unsigned int i;
	int ret = -ENODEV;
	uint32_t iommus[num_utlbs * 2];

	ret = of_get_u32_array(node, "iommus", iommus, ARRAY_SIZE(iommus));
	if (ret != ARRAY_SIZE(iommus))
		return -ENODEV;

	spin_lock(&ipmmu_devices_lock);

	for (i = 0; i < num_utlbs; ++i) {
		struct ipmmu_device *mmu;
		uint32_t phandle;

		ret = -ENODEV;
		list_for_each_entry (mmu, &ipmmu_devices, list) {
			of_get_u32_array(mmu->node, "phandle", &phandle, 1);
			if (!phandle || phandle != iommus[i * 2])
				continue;

			/*
			 * TODO Take a reference to the MMU to protect
			 * against device removal.
			 */
			ret = 0;
			break;
		}
		if (ret < 0)
			break;

		utlbs[i].utlb = iommus[i * 2 + 1];
		utlbs[i].mmu = mmu;
	}

	spin_unlock(&ipmmu_devices_lock);

	return ret;
}

static int ipmmu_node_init(struct device_node *node)
{
	struct node_ipmmu *node_ipmmu;
	struct ipmmu_device *mmus[IPMMU_PER_DEV_MAX];
	struct ipmmu_utlb *utlbs;
	unsigned int i;
	int num_utlbs;
	int num_mmus;
	int ret;
	of32_t *iommus;
	int len;

	/* Find the master corresponding to the device. */

	iommus = of_getprop(node, "iommus", &len);
	if (!iommus || len < sizeof(*iommus))
		return -ENODEV;

	num_utlbs = len / sizeof(*iommus) / 2;

	utlbs = zalloc(num_utlbs * sizeof(*utlbs));
	if (!utlbs)
		return -ENOMEM;

	ret = ipmmu_find_utlbs(node, utlbs, num_utlbs);
	if (ret < 0)
		goto error;

	num_mmus = 0;
	for (i = 0; i < num_utlbs; i++) {
		if (!utlbs[i].mmu || utlbs[i].utlb >= utlbs[i].mmu->num_utlbs) {
			ret = -EINVAL;
			goto error;
		}

		if (!num_mmus || mmus[num_mmus - 1] != utlbs[i].mmu) {
			if (num_mmus >= IPMMU_PER_DEV_MAX) {
				ret = -EINVAL;
				goto error;
			} else {
				num_mmus++;
				mmus[num_mmus - 1] = utlbs[i].mmu;
			}
		}
	}

	node_ipmmu = zalloc(sizeof(*node_ipmmu));
	if (!node_ipmmu) {
		ret = -ENOMEM;
		goto error;
	}

	node_ipmmu->num_mmus = num_mmus;
	memcpy(node_ipmmu->mmus, mmus, num_mmus * sizeof(*mmus));
	node_ipmmu->utlbs = utlbs;
	node_ipmmu->num_utlbs = num_utlbs;
	node_ipmmu->node = node;
	node->iommu.priv = node_ipmmu;

	/* Minos: */
	pr_notice("%s: Initialized master device (IPMMUs %u micro-TLBs %u)\n",
		  devnode_name(node), num_mmus, num_utlbs);
	for (i = 0; i < num_mmus; i++)
		pr_notice("%s: IPMMU%d: %s\n", devnode_name(node), i + 1,
			  devnode_name(mmus[i]->node));

	return 0;

error:
	free(utlbs);
	return ret;
}

/*
 * Probe/remove and init
 */

static void ipmmu_device_reset(struct ipmmu_device *mmu)
{
	unsigned int i;

	/* Disable all contexts. */
	for (i = 0; i < mmu->num_ctx; ++i)
		ipmmu_write(mmu, i * IM_CTX_SIZE + IMCTR, 0);
}

/*
 * Minos: We don't have refcount for allocated memory so manually free memory
 * when an error occured.
 */
static int ipmmu_probe(struct device_node *node)
{
	struct ipmmu_device *mmu;
	phy_addr_t address;
	size_t size;
	int len;
	uint32_t irq;
	unsigned long flags;
	int ret;

	mmu = zalloc(sizeof(*mmu));
	if (!mmu) {
		pr_err("%s: cannot allocate device data\n", devnode_name(node));
		return -ENOMEM;
	}

	mmu->node = node;
	mmu->num_utlbs = 48;
	spin_lock_init(&mmu->lock);
	bitmap_zero(mmu->ctx, IPMMU_CTX_MAX);

	/* Map I/O memory and request IRQ. */
	ret = of_translate_address(node, &address, &size);
	if (ret || !size) {
		ret = -ENOENT;
		goto out;
	}

	mmu->base = (void *)io_remap(address, size);
	if (!mmu->base) {
		ret = -ENOMEM;
		goto out;
	}

	/*
	 * The number of contexts varies with generation and instance.
	 * Newer SoCs get a total of 8 contexts enabled, older ones just one.
	 */
	mmu->num_ctx = 8;

	BUG_ON(mmu->num_ctx > IPMMU_CTX_MAX);

	/*
	 * Determine if this IPMMU instance is a leaf device by checking
	 * if the renesas,ipmmu-main property exists or not.
	 */
	if (of_getprop(node, "renesas,ipmmu-main", &len))
		mmu->is_leaf = true;

	/* Root devices have mandatory IRQs */
	if (ipmmu_is_root(mmu)) {
		ret = get_device_irq_index(node, &irq, &flags, 0);
		if (ret) {
			pr_err("%s: no IRQ found\n", devnode_name(node));
			goto out;
		}

		ret = request_irq(irq, ipmmu_irq, flags, NULL, mmu);
		if (ret) {
			pr_err("%s: failed to request IRQ %d\n",
			       devnode_name(node), irq);
			goto out;
		}

		ipmmu_device_reset(mmu);

		/* Use stage 2 translation table format */
		ipmmu_write(mmu, IMSAUXCTLR,
			    ipmmu_read(mmu, IMSAUXCTLR) | IMSAUXCTLR_S2PTE);
	} else {
		/* Only IPMMU caches are affected */

		/*
		 * Disable IPMMU TLB cache function of IPMMU caches
		 * that do require such action.
		 */
		if (ipmmu_is_mmu_tlb_disable_needed(node))
			ipmmu_write(mmu, IMSCTLR,
				    ipmmu_read(mmu, IMSCTLR) |
					    IMSCTLR_DISCACHE);
	}

	spin_lock(&ipmmu_devices_lock);
	list_add(&ipmmu_devices, &mmu->list);
	spin_unlock(&ipmmu_devices_lock);

	/* Minos: */
	pr_notice("%s: registered %s IPMMU\n", devnode_name(node),
		  ipmmu_is_root(mmu) ? "root" : "cache");

	return 0;

out:
	if (!mmu->base)
		io_unmap((unsigned long)mmu->base, size);
	free(mmu);

	return ret;
}

/*
 * Start of Minos specific code
 */

static int ipmmu_iotlb_flush_all(struct vm *vm)
{
	struct vm_ipmmu *vm_ipmmu = to_vm_ipmmu(vm);

	if (!vm_ipmmu || !vm_ipmmu->base_context)
		return 0;

	spin_lock(&vm_ipmmu->lock);
	ipmmu_tlb_invalidate(to_ipmmu_domain(vm_ipmmu->base_context));
	spin_unlock(&vm_ipmmu->lock);
	return 0;
}

static struct iommu_domain *ipmmu_get_domain(struct vm *vm,
					     struct device_node *node)
{
	struct vm_ipmmu *vm_ipmmu = to_vm_ipmmu(vm);
	struct iommu_domain *io_domain;

	if (!to_node_ipmmu(node)->mmus[0] || !to_node_ipmmu(node)->num_mmus)
		return NULL;

	/*
	 * Loop through the &vm_ipmmu->contexts to locate a context
	 * assigned to this IPMMU
	 */
	list_for_each_entry (io_domain, &vm_ipmmu->contexts, list) {
		if (ipmmus_are_equal(to_ipmmu_domain(io_domain),
				     to_node_ipmmu(node)))
			return io_domain;
	}

	return NULL;
}

static void ipmmu_destroy_domain(struct iommu_domain *io_domain)
{
	struct ipmmu_domain *domain = to_ipmmu_domain(io_domain);

	list_del(&io_domain->list);

	if (domain->num_mmus) {
		/*
		 * Disable the context for cache IPMMU only. Flush the TLB as required
		 * when modifying the context registers.
		 */
		ipmmu_ctx_write_cache(domain, IMCTR, IMCTR_FLUSH);
	} else {
		/*
		 * Free main domain resources. We assume that all devices have already
		 * been detached.
		 */
		ipmmu_domain_destroy_context(domain);
	}

	free(domain);
}

static int ipmmu_alloc_page_table(struct vm *vm);

static int ipmmu_assign_node(struct vm *vm, struct device_node *node)
{
	struct vm_ipmmu *vm_ipmmu = to_vm_ipmmu(vm);
	struct iommu_domain *io_domain;
	struct ipmmu_domain *domain;
	int ret = 0;

	if (!vm_ipmmu)
		return -EINVAL;

	if (!vm_ipmmu->base_context) {
		ret = ipmmu_alloc_page_table(vm);
		if (ret)
			return ret;
	}

	if (!to_node_ipmmu(node)) {
		ret = ipmmu_node_init(node);
		if (ret)
			return ret;
	}

	spin_lock(&vm_ipmmu->lock);

	if (to_node_ipmmu(node)->io_domain) {
		pr_err("%s: already attached to IPMMU domain\n",
		       devnode_name(node));
		ret = -EEXIST;
		goto out;
	}

	/*
	 * Check to see if a context bank (iommu_domain) already exists for
	 * this Minos VM under the same IPMMU
	 */
	io_domain = ipmmu_get_domain(vm, node);
	if (!io_domain) {
		domain = zalloc(sizeof(*domain));
		if (!domain) {
			ret = -ENOMEM;
			goto out;
		}
		spin_lock_init(&domain->lock);

		domain->vm = vm;
		domain->context_id =
			to_ipmmu_domain(vm_ipmmu->base_context)->context_id;
		io_domain = &domain->io_domain;

		/* Chain the new context to the Minos VM */
		list_add(&vm_ipmmu->contexts, &io_domain->list);
	}

	ret = ipmmu_attach_node(io_domain, node);
	if (ret) {
		if (atomic_read(&io_domain->ref) == 0)
			ipmmu_destroy_domain(io_domain);
	} else {
		atomic_inc(&io_domain->ref);
		to_node_ipmmu(node)->io_domain = io_domain;
	}

out:
	spin_unlock(&vm_ipmmu->lock);

	return ret;
}

static int ipmmu_alloc_page_table(struct vm *vm)
{
	struct vm_ipmmu *vm_ipmmu = to_vm_ipmmu(vm);
	struct ipmmu_domain *domain;
	struct ipmmu_device *root;
	int ret;

	root = ipmmu_find_root(NULL);
	if (!root) {
		pr_err("vm%d: Unable to locate root IPMMU\n", vm_id(vm));
		return -EAGAIN;
	}

	domain = zalloc(sizeof(*domain));
	if (!domain)
		return -ENOMEM;

	spin_lock_init(&domain->lock);
	init_list(&domain->io_domain.list);
	domain->vm = vm;
	domain->root = root;
	/* Clear num_mmus explicitly. */
	domain->num_mmus = 0;

	spin_lock(&vm_ipmmu->lock);
	ret = ipmmu_domain_init_context(domain);
	if (ret < 0) {
		pr_err("%s: vm%d: Unable to initialize IPMMU context\n",
		       devnode_name(root->node), vm_id(vm));
		spin_unlock(&vm_ipmmu->lock);
		free(domain);
		return ret;
	}
	vm_ipmmu->base_context = &domain->io_domain;
	spin_unlock(&vm_ipmmu->lock);

	return 0;
}

static int ipmmu_vm_init(struct vm *vm)
{
	struct vm_ipmmu *vm_ipmmu;

	vm_ipmmu = zalloc(sizeof(*vm_ipmmu));
	if (!vm_ipmmu)
		return -ENOMEM;

	spin_lock_init(&vm_ipmmu->lock);
	init_list(&vm_ipmmu->contexts);

	vm->iommu.priv = vm_ipmmu;

	return 0;
}

static void *populate_ipmmu_masters(struct device_node *node, void *arg)
{
	struct device_node *ipmmu_node = arg;
	uint32_t phandle;
	uint32_t iommus;

	of_get_u32_array(ipmmu_node, "phandle", &phandle, 1);
	of_get_u32_array(node, "iommus", &iommus, 1);

	if (!phandle || phandle != iommus)
		return NULL;

	pr_notice("%s: found master device %s\n", devnode_name(ipmmu_node),
		  devnode_name(node));

	return node;
}

static int ipmmu_init(struct device_node *node)
{
	int rc;

	/*
	 * Perform platform specific actions such as power-on, errata maintenance
	 * if required.
	 */
	rc = ipmmu_power_on(node);
	if (rc) {
		pr_err("%s: failed to preinit IPMMU (%d)\n", devnode_name(node),
		       rc);
		return rc;
	}

	rc = ipmmu_probe(node);
	if (rc) {
		pr_err("%s: failed to init IPMMU\n", devnode_name(node));
		return rc;
	}

	of_iterate_all_node_loop(hv_node, populate_ipmmu_masters, node);

	return 0;
}

static struct iommu_ops ipmmu_ops = {
	.init = ipmmu_init,
	.vm_init = ipmmu_vm_init,
	.iotlb_flush_all = ipmmu_iotlb_flush_all,
	.assign_node = ipmmu_assign_node,
};

IOMMU_OPS_DECLARE(ipmmu_ops, ipmmu_match_table, (void *)&ipmmu_ops);
