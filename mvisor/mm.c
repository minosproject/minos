/*
 * Created by Le Min 2017/12/12
 */

#include <mvisor/types.h>
#include <config/config.h>
#include <mvisor/spinlock.h>

static char *free_mem_base = NULL;
static phy_addr_t free_mem_size = 0;
static spinlock_t mem_block_lock;

/*
 * for 4K page allocation
 */
static char *free_4k_base = 0;

extern unsigned char __code_start;
extern unsigned char __code_end;

int vmm_mm_init(void)
{
	size_t size;

	spin_lock_init(&mem_block_lock);
	size = (&__code_end) - (&__code_start);
	size = BALIGN(size, sizeof(unsigned long));
	free_mem_base = (char *)(CONFIG_MVISOR_START_ADDRESS + size);
	free_mem_size = CONFIG_MVISOR_RAM_SIZE - size;

	/*
	 * assume the memory region is 4k align
	 */
	free_4k_base = free_mem_base + free_mem_size;
}

char *vmm_malloc(size_t size)
{
	size_t request_size;
	char *base;

	request_size = BALIGN(size, sizeof(unsigned long));

	spin_lock(&mem_block_lock);

	if (free_mem_size < request_size)
		base = NULL;
	else
		base = free_mem_base;

	free_mem_base += request_size;
	free_mem_size -= request_size;

	spin_unlock(&mem_block_lock);

	return base;
}

char *vmm_alloc_pages(int pages)
{
	size_t request_size = pages * SIZE_4K;
	char *base;

	if (pages <= 0)
		return NULL;

	spin_lock(&mem_block_lock);
	if (free_mem_size < request_size)
		return NULL;

	if (((phy_addr_t)free_4k_base - request_size) < free_mem_size)
		return NULL;

	base = free_4k_base - request_size;
	free_4k_base = base;
	free_mem_size -= request_size;
	spin_unlock(&mem_block_lock);

	return base;
}
