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

	__vmm_vm_start = .;
	.__vmm_vm : {
		*(.__vmm_vm)
	}
	__vmm_vm_end = .;

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
	. = __percpu_cpu_0_end + __percpu_section_size * (4 - 1);
	__percpu_end = .;

	. = ALIGN(4096);
	__bss_start = .;
	.bss : {*(.bss)}
	__bss_end = .;

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

	__init_end = .;
	__code_end = .;
}
