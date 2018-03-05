#include <config/config.h>

ENTRY(_start)
SECTIONS
{
	.vectors 0x80000000:
	{
		__code_start = .;
		KEEP(*(__start_up))
		KEEP(*(__el3_vectors __el2_vectors __int_handlers))
		KEEP(*(__armv8_cpu))
	}

	.text : 
	{
		*(.text) 
		*(.rodata)
	}

	. = ALIGN(8);

	.data : {*(.data)}

	. = ALIGN(8);

	__vmm_module_start = .;
	.__vmm_module : {
		*(.__vmm_module)
	}
	__vmm_module_end = .;

	. = ALIGN(8);

	__vmm_vm_start = .;
	.__vmm_vm : {
		*(.__vmm_vm)
	}
	__vmm_vm_end = .;

	. = ALIGN(8);

	__bss_start = .;
	.bss : {*(.bss)}
	__bss_end = .;

	. = ALIGN(8);

	.el2_stack (NOLOAD): {
		. = ALIGN(64);
		__el2_stack = .;
		. = . + (CONFIG_NR_CPUS * 0x2000);
		__el2_stack_end = .;
	}

	.el3_stack (NOLOAD): {
		. = ALIGN(64);
		__el3_stack = .;
		. = . + (CONFIG_NR_CPUS * 0x1000);
		__el3_stack_end = .;
	}

	. = ALIGN(4096);

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

	.el2_stage2_ttb_l1 (NOLOAD): {
		. = ALIGN(MMU_TTB_LEVEL1_ALIGN);
		__el2_stage2_ttb_l1 = .;
		. = . + (CONFIG_NR_CPUS * MMU_TTB_LEVEL1_SIZE);
		__el2_stage2_ttb_l1_end = .;
	}

	.el2_stage2_ttbl2 (NOLOAD): {
		. = ALIGN(MMU_TTB_LEVEL2_ALIGN);
		__el2_stage2_ttb_l2 = .;
		. = . + (CONFIG_NR_CPUS * MMU_TTB_LEVEL2_SIZE);
		__el2_stage2_ttb_l2_end = .;
	}

	. = ALIGN(8);

	__init_start = .;

	__init_func_start = .;
	.__init_func_0 : {
		*(.__init_func_0)
	}
	.__init_func_1 : {
		*(.__init_func_1)
	}
	.__init_func_2 : {
		*(.__init_func_2)
	}
	.__init_func_3 : {
		*(.__init_func_3)
	}
	.__init_func_4 : {
		*(.__init_func_4)
	}
	.__init_func_5 : {
		*(.__init_func_5)
	}
	.__init_func_6 : {
		*(.__init_func_6)
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
	__vmm_irq_resource_start = .;
	.__vmm_irq_resource : {
		*(.__vmm_irq_resource)
	}
	__vmm_irq_resource_end = .;

	. = ALIGN(8);
	__vmm_memory_resource_start = .;
	.__vmm_memory_resource : {
		*(.__vmm_memory_resource)
	}
	__vmm_memory_resource_end = .;

	. = ALIGN(8);

	__init_end = .;

	__code_end = .;
}
