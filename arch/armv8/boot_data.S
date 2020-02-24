	.data

	.global code_start
	.global code_end

code_start	: .long __code_start
code_end	: .long __code_end
vmm_vm_start	: .long __vmm_vm_start
vmm_vm_end	: .long __vmm_vm_end
