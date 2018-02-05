#include <mvisor/mvisor.h>
#include <config/vm_config.h>

struct irq_config irq_config_table[] = {
	{32, 512, 0, 0, 0, "watchdog"},
	{34, 513, 0, 0, 0, "sp804-timer0"},
	{35, 514, 0, 0, 0, "sp804-timer1"},
	{36, 515, 0, 0, 0, "pl031-rtc"},
	{37, 516, 0, 0, 0, "pl011-uart0"},
	{38, 517, 0xff, 0, 0, "pl011-uart1"},
};

int get_irq_config_size(void)
{
	return (sizeof(irq_config_table) /
		sizeof(irq_config_table[0]));
}

void *get_irq_config_table(void)
{
	return (void *)irq_config_table;
}
