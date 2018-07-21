#include <config/config.h>

ENTRY(_start)
SECTIONS
{
	.vectors 0xc0000000:
	{
		/*
		 * put all asm code into this section
		 */
		__code_start = .;
		KEEP(*(__start_up))
		KEEP(*(__el3_vectors __el2_vectors __int_handlers __asm_code))
	}

	.text : 
	{
		*(.text) 
	}

	. = ALIGN(8);

	.data : {*(.data)}

	. = ALIGN(8);

	.smp_holding_pen : {
		__smp_hoding_pen = .;
		. = . + (CONFIG_NR_CPUS * 8);
		__smp_hoding_pen_end = .;
	}

	. = ALIGN(8);

	__percpu_start = .;
	__percpu_cpu_0_start = .;
	.percpu_0 : {
		KEEP(*(".__percpu"))
	}
	. = ALIGN(64);
	__percpu_cpu_0_end = .;
	__percpu_section_size = __percpu_cpu_0_end - __percpu_cpu_0_start;

	.__percpu_others : {

	}
	. = __percpu_cpu_0_end + __percpu_section_size * (CONFIG_NR_CPUS - 1);
	__percpu_end = .;

	. = ALIGN(8);

	__bss_start = .;
	.bss : {*(.bss)}
	__bss_end = .;

	. = ALIGN(8);

	__vmodule_start = .;
	.__vmodule : {
		*(.__vmodule)
	}
	__vmodule_end = .;

	. = ALIGN(8);

	__irqchip_start = .;
	.__irqchip : {
		*(.__irqchip)
	}
	__irqchip_end = .;

	. = ALIGN(8);

	__mmuchip_start = .;
	.__mmuchip : {
		*(.__mmuchip)
	}
	__mmuchip_end = .;

	. = ALIGN(8);

	__vm_start = .;
	.__vm : {
		*(.__vm)
	}
	__vm_end = .;

	. = ALIGN(8);

	__serror_desc_start = .;
	.__serror_desc : {
		*(.__serror_desc)
	}
	__serror_desc_end = .;

	. = ALIGN(8);

	__smc_handler_start = .;
	.__smc_handler : {
		*(.__smc_handler)
	}
	__smc_handler_end = .;

	. = ALIGN(8);

	__hvc_handler_start = .;
	.__hvc_handler : {
		*(.__hvc_handler)
	}
	__hvc_handler_end = .;

	. = ALIGN(8);

	__static_vcpu_info_start = .;
	.__static_vcpu_info : {
		*(.__static_vcpu_info)
	}
	__static_vcpu_info_end = .;

	. = ALIGN(8);

	__init_start = .;

	__init_func_start = .;
	__init_func_0_start = .;
	.__init_func_0 : {
		*(.__init_func_0)
	}
	__init_func_1_start = .;
	.__init_func_1 : {
		*(.__init_func_1)
	}
	__init_func_2_start = .;
	.__init_func_2 : {
		*(.__init_func_2)
	}
	__init_func_3_start = .;
	.__init_func_3 : {
		*(.__init_func_3)
	}
	__init_func_4_start = .;
	.__init_func_4 : {
		*(.__init_func_4)
	}
	__init_func_5_start = .;
	.__init_func_5 : {
		*(.__init_func_5)
	}
	__init_func_6_start = .;
	.__init_func_6 : {
		*(.__init_func_6)
	}
	__init_func_7_start = .;
	.__init_func_7 : {
		*(.__init_func_7)
	}
	__init_func_8_start = .;
	.__init_func_8 : {
		*(.__init_func_8)
	}
	__init_func_9_start = .;
	.__init_func_9 : {
		*(.__init_func_9)
	}
	__init_func_end = .;

	. = ALIGN(8);

	__init_data_start = .;
	.__init_data_section : {
		*(.__init_data_section)
	}
	__init_data_end = .;

	. = ALIGN(8);

	__init_text_start = .;
	.__init_text : {
		*(__init_text)
	}
	__init_text_end = .;

	. = ALIGN(8);

	__init_end = .;

	. = ALIGN(16);
	__config_data_start = .;
	.__config : {
		*(.__config)
	}
	__config_data_end = .;
	. = ALIGN(16);

	.el2_stack : {
		. = ALIGN(64);
		__el2_stack = .;
		. = . + (CONFIG_NR_CPUS * 0x2000);
		__el2_stack_end = .;
	}

	.el3_stack : {
		. = ALIGN(64);
		__el3_stack = .;
		. = . + (CONFIG_NR_CPUS * 0x100);
		__el3_stack_end = .;
	}

	.el2_ttb0_pgd : {
		. = ALIGN(4096);
		__el2_ttb0_pgd = .;
		. = . + 0x1000;
	}

	/* 4K level1 can map 512GB memory */
	.el2_ttb0_pud : {
		. = ALIGN(4096);
		__el2_ttb0_pud = .;
		. = . + 0x1000;
	}

	.el2_ttb0_pmd_code : {
		. = ALIGN(4096);
		__el2_ttb0_pmd_code = .;
		. = . + 0x1000;
	}

	.el2_ttb0_pmd_io : {
		. = ALIGN(4096);
		__el2_ttb0_pmd_io = .;
		. = . + 0x1000;
	}

	__symbols_start = .;

	.__symbols__ : {
		*(.__symbols__)
	}

	.rodata : {
		*(.rodata)
	}

	__code_end = .;
}
