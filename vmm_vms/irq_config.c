#include <mvisor/resource.h>
#include <mvisor/init.h>
#include <mvisor/irq.h>

/* hno vno vmid affinity type name */
struct irq_resource irq_resource_table[] __irq_resource = {
	{13, 13, 0xffff, 0, IRQ_FLAG_TYPE_LEVEL_LOW, "vmm_reched"},
	{32, 32, 0, 0, IRQ_FLAG_TYPE_LEVEL_LOW, "watchdog"},
	{34, 34, 0, 0, IRQ_FLAG_TYPE_LEVEL_LOW, "sp804-timer0"},
	{35, 35, 0, 0, IRQ_FLAG_TYPE_LEVEL_LOW, "sp804-timer1"},
	{36, 36, 0, 0, IRQ_FLAG_TYPE_LEVEL_LOW, "pl031-rtc"},
	{37, 37, 0, 0, IRQ_FLAG_TYPE_LEVEL_LOW, "pl011-uart0"},
	{38, 38, 0xffff, 0, IRQ_FLAG_TYPE_LEVEL_LOW, "pl011-uart1"},
};
