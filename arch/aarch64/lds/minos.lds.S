#include <config/config.h>

ENTRY(_start)
SECTIONS
{
	.vectors CONFIG_MINOS_ENTRY_ADDRESS:
	{
		/*
		 * put all asm code into this section
		 */
		__code_start = .;
		KEEP(*(__start_up))
		KEEP(*(__el2_vectors __int_handlers __asm_code))
	}

	.text :
	{
		*(.text)
	}

	. = ALIGN(8);

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

	. = ALIGN(8);

	__sync_desc_start = .;
	.__sync_desc : {
		*(.__sync_desc)
	}
	__sync_desc_end = .;

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

	__task_desc_start = .;
	.__task_desc : {
		*(.__task_desc)
	}
	__task_desc_end = .;

	. = ALIGN(8);

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

	.irq_stack : {
		. = ALIGN(64);
		__irq_stack_start = .;
		. = . + (CONFIG_NR_CPUS * CONFIG_EXCEPTION_SIZE);
		__irq_stack_end = .;
	}

	.el2_ttb0_pgd : {
		. = ALIGN(4096);
		__el2_ttb0_pgd = .;
		. = . + 0x1000;
	}

	/*
	 * 4K level1 can map 512GB memory, for now
	 * in hypervisor, just map max to 4GB when boot
	 */
	.el2_ttb0_pud : {
		. = ALIGN(4096);
		__el2_ttb0_pud = .;
		. = . + 0x1000;
	}

	/* max map 4GB pmd when booting */
	.el2_ttb0_pmd : {
		. = ALIGN(4096);
		__el2_ttb0_pmd = .;
		. = . + 0x4000;
	}

	__symbols_start = .;

	.__symbols__ : {
		*(.__symbols__)
	}

	.rodata : {
		*(.rodata)
	}
	.rodata.str1.8 : {
		*(.rodata.str1.8)
	}

	__code_end = .;
}
