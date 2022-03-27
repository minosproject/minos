#ifndef __MINOS_CPU_H__
#define __MINOS_CPU_H__

#include <asm/psci.h>

int psci_cpu_on(unsigned long cpu, unsigned long entry);
int psci_cpu_off(unsigned long cpu);
void psci_system_reboot(int mode, const char *cmd);
void psci_system_shutdown(void);

int psci_cpu_on_hvc(unsigned long cpu, unsigned long entry);
int psci_cpu_off_hvc(unsigned long cpu);
void psci_system_reboot_hvc(int mode, const char *cmd);
void psci_system_shutdown_hvc(void);

int spin_table_cpu_on(unsigned long affinity, unsigned long entry);

#endif
