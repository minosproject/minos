#ifndef __MINOS_CPU_H__
#define __MINOS_CPU_H__

int psci_cpu_on(unsigned long cpu, unsigned long entry);
int psci_cpu_off(unsigned long cpu);
void psci_system_reboot(int mode, const char *cmd);
void psci_system_shutdown(void);

int spin_table_cpu_on(unsigned long affinity, unsigned long entry);

#endif
