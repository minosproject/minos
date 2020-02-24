/*
 * Created by Le Min 2017/12/12
 */

#include <core/types.h>
#include <config/mvisor_config.h>
#include <asm/cpu.h>
#include <core/spinlock.h>

static char *free_mem_base = NULL;
static uint64_t free_mem_size = 0;
static spinlock_t mem_block_lock;

extern uint64_t code_start;
extern uint64_t code_end;

int init_mem_block(void)
{
	spin_lock_init(&mem_block_lock);
}

char *request_free_mem(uint64_t size)
{
	spin_lock(&mem_block_lock);
	spin_unlock(&mem_block_lock);
	return free_mem_base;
}
