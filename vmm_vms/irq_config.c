#include <mvisor/resource.h>
#include <mvisor/init.h>

struct irq_resource irq_resource_table[] __irq_resource = {
	{32, 512, 0, 0, 0, "watchdog"},
	{34, 513, 0, 0, 0, "sp804-timer0"},
	{35, 514, 0, 0, 0, "sp804-timer1"},
	{36, 515, 0, 0, 0, "pl031-rtc"},
	{37, 516, 0, 0, 0, "pl011-uart0"},
	{38, 517, 0xffff, 0, 0, "pl011-uart1"},
};
