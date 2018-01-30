#ifndef _MVISOR_GIC_H_
#define _MVISOR_GIC_H_

#include <asm/gicv3.h>

int gic_gicd_global_init(void);
int gic_gicr_global_init(void);

void config_gicd(uint32_t value);
void enable_gicd(uint32_t flags);
void disable_gicd(uint32_t flags);
void sync_are_in_gicd(int flags, uint32_t dosync);
void enable_spi(uint32_t id);
void disable_spi(uint32_t id);
void set_spi_int_priority(uint32_t id, uint32_t priority);
uint32_t get_spi_int_priority(uint32_t id);
void set_spi_int_route(uint32_t id, uint32_t affinity, uint64_t mode);
void set_spi_int_target(uint32_t id, uint32_t target);
uint32_t get_spi_int_target(uint32_t id);
void config_spi_int(uint32_t id, uint32_t config);
void set_spi_int_pending(uint32_t id);
void clear_spi_int_pending(uint32_t id);
uint32_t get_spi_int_pending(uint32_t id);
void set_spi_int_sec(uint32_t id, int group);
void set_spi_int_sec_block(uint32_t block, uint32_t group);
void set_spi_int_sec_all(uint32_t group);
uint64_t get_spi_route(uint32_t id);

void wakeup_gicr(void);
void enable_private_int(uint32_t id);
void set_private_int_priority(uint32_t id, uint32_t priority);
uint32_t get_private_int_priority(uint32_t id);
void set_private_int_pending(uint32_t id);
void clear_private_int_pending(uint32_t id);
uint32_t get_private_int_pending(uint32_t id);
void set_private_int_sec(uint32_t id, int group);
void set_private_int_sec_block(int group);

int gic_local_init(void);
int gic_global_init(void);

void send_sgi_int_to_cpu(uint32_t cpu, uint8_t intid);
void send_sgi_int_to_all(uint8_t intid);

#endif
