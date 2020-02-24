ENTRY(_start)
SECTIONS
{
	. = 0x80000000;

	__code_start = .;
	.text : 
	{
		*(.text) 
		*(.rodata)
	}

	. = ALIGN(4);

	.data : {*(.data)}

	. = ALIGN(4);

	__bss_start = .;
	.bss : {*(.bss)}
	__bss_end = .;

	. = ALIGN(4);

	__vmm_vm_start = .;
	.__vmm_vm : {
		*(.__vmm_vm)
	}
	__vmm_vm_end = .;

	__init_start = .;

	. = ALIGN(4);

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

	. = ALIGN(4);

	__init_data_start = .;
	.__init_data_section : {
		*(.__init_data_section)
	}
	__init_data_end = .;

	. = ALIGN(4);

	__init_text_start = .;
	.__init_text : {
		*(__init_text)
	}
	__init_text_end = .;

	. = ALIGN(4);

	__init_end = .;
	__code_end = .;
}
