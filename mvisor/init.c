#include <mvisor/init.h>
#include <mvisor/types.h>
#include <mvisor/print.h>
#include <asm/arch.h>

extern unsigned char __init_func_0_start;
extern unsigned char __init_func_1_start;
extern unsigned char __init_func_2_start;
extern unsigned char __init_func_3_start;
extern unsigned char __init_func_4_start;
extern unsigned char __init_func_5_start;
extern unsigned char __init_func_6_start;

static void call_init_func(unsigned long fn_start, unsigned long fn_end)
{
	init_call *fn;
	int size, i;

	size = (fn_end - fn_start) / sizeof(init_call);
	pr_info("call init func : 0x%x 0x%x %d\n", fn_start, fn_end, size);

	if (size <= 0)
		return;

	fn = (init_call *)fn_start;
	for (i = 0; i < size; i++) {
		(*fn)();
		fn++;
	}
}

void vmm_arch_init(void)
{
	arch_init();

	call_init_func((unsigned long)&__init_func_1_start,
			(unsigned long)&__init_func_2_start);
}

void vmm_early_init(void)
{
	arch_early_init();

	call_init_func((unsigned long)&__init_func_0_start,
			(unsigned long)&__init_func_1_start);
}

void vmm_devices_init(void)
{
	call_init_func((unsigned long)&__init_func_4_start,
			(unsigned long)&__init_func_5_start);
}
