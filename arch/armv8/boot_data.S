	.data

	.global code_start
	.global code_end
	.global vmm_vm_start
	.global vmm_vm_end

	.align 10

code_start	:
	.word __code_start
	.word 0x00000000

code_end	:
	.word __code_end
	.word 0x00000000

vmm_vm_start	:
	.word __vmm_vm_start
	.word 0x00000000

vmm_vm_end	:
	.word __vmm_vm_end
	.word 0x00000000

