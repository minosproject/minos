#include <config/config.h>

ENTRY(_start)
SECTIONS
{
	.vectors CONFIG_MINOS_ENTRY_ADDRESS + CONFIG_PTOV_MASK:
	{
		/*
		 * put all asm code into this section
		 */
		__minos_start = .;
		__code_start = .;
		KEEP(*(__start_up))
		KEEP(*(__elx_vectors __int_handlers __asm_code))
	}

	.text :
	{
		*(.text)
	}

	. = ALIGN(4096);
	__code_end = .;

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

	. = ALIGN(4096);
	__init_end = .;

	__data_start = .;
	.stage1_page_table : {
		. = ALIGN(4096);
		__stage1_page_table = .;
		. = . + 0x1000;
	}

	.data : {*(.data)}

	. = ALIGN(8);

	.smp_affinity_id : {
		__smp_affinity_id = .;
		. = . + (CONFIG_NR_CPUS * 8);
		__smp_affinity_id_end = .;
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

	__platform_start = .;
	.__platform : {
		*(.__platform)
	}
	__platform_end = .;

	. = ALIGN(8);

	__irqchip_start = .;
	.__irqchip : {
		*(.__irqchip)
	}
	__irqchip_end = .;

	. = ALIGN(8);

	__iommu_ops_start = .;
	.__iommu_ops : {
		*(.__iommu_ops)
	}
	__iommu_ops_end = .;

	. = ALIGN(8);

	__virqchip_start = .;
	.__virqchip : {
		*(.__virqchip)
	}
	__virqchip_end = .;

	. = ALIGN(8);

	__vdev_start = .;
	.__vdev : {
		*(.__vdev)
	}
	__vdev_end = .;

	__console_start = .;
	.__console : {
		*(.__console)
	}
	__console_end = .;

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

	__shell_command_start = .;
	.__shell_command : {
		*(.__shell_command)
	}
	__shell_command_end = .;

	. = ALIGN(4096);
	__data_end = .;

	__rodata_start = .;
	__symbols_start = .;
	.__symbols__ : {
		KEEP(*(.__symbols__))
	}

	.rodata : {
		KEEP(*(.rodata))
	}
	.rodata.str1.8 : {
		KEEP(*(.rodata.str1.8))
	}

	__minos_end = .;
}
