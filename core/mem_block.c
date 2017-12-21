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

extern unsigned char *code_start;
extern unsigned char *code_end;

int init_mem_block(void)
{
	size_t size;

	spin_lock_init(&mem_block_lock);
	size = code_end - code_start;
	size = ALIGN(size, sizeof(uint64_t));
	free_mem_base = (char *)(MVISOR_START_ADDRESS + size);
	free_mem_size = MVISOR_RAM_SIZE - size;
}

char *request_free_mem(uint64_t size)
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
