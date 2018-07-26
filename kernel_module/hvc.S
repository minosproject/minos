#include <linux/linkage.h>
#include <asm/assembler.h>

ENTRY(__minos_hvc)
	hvc 	0
	mov	x4, x1
	mov	x5, x2
	mov	x6, x3
	ret
ENDPROC(__minos_hvc)

ENTRY(minos_hvc_result1)
	str	x4, [x0]
	ret
ENDPROC(minos_hvc_result1)

ENTRY(minos_hvc_result2)
	str	x4, [x0]
	str	x5, [x1]
	ret
ENDPROC(minos_hvc_result2)

ENTRY(minos_hvc_result3)
	str	x4, [x0]
	str	x5, [x1]
	str	x6, [x2]
	ret
ENDPROC(minos_hvc_result3)
