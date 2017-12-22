/*
 * Created by Le Min 2017/12/12
 */

#include <core/types.h>
#include <config/mvisor_config.h>
#include <asm/cpu.h>
#include <core/spinlock.h>

static char *free_mem_base = NULL;
static phy_addr_t free_mem_size = 0;
static spinlock_t mem_block_lock;

extern unsigned char __code_start;
extern unsigned char __code_end;

int init_mem_block(void)
{
	size_t size;

	spin_lock_init(&mem_block_lock);
	size = (&__code_end) - (&__code_start);
	size = ALIGN(size, sizeof(unsigned long));
	free_mem_base = (char *)(CONFIG_MVISOR_START_ADDRESS + size);
	free_mem_size = CONFIG_MVISOR_RAM_SIZE - size;
}

char *vmm_malloc(size_t size)
{
	size_t request_size;
	char *base;

	request_size = ALIGN(size, sizeof(unsigned long));

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
