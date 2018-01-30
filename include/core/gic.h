#ifndef _MVISOR_GIC_H_
#define _MVISOR_GIC_H_

#include <core/gicv3.h>

/**
 * IRQ line type.
 *
 * IRQ_TYPE_NONE            - default, unspecified type
 * IRQ_TYPE_EDGE_RISING     - rising edge triggered
 * IRQ_TYPE_EDGE_FALLING    - falling edge triggered
 * IRQ_TYPE_EDGE_BOTH       - rising and falling edge triggered
 * IRQ_TYPE_LEVEL_HIGH      - high level triggered
 * IRQ_TYPE_LEVEL_LOW       - low level triggered
 * IRQ_TYPE_LEVEL_MASK      - Mask to filter out the level bits
 * IRQ_TYPE_SENSE_MASK      - Mask for all the above bits
 * IRQ_TYPE_INVALID         - Use to initialize the type
 */
#define IRQ_TYPE_NONE           	0x00000000
#define IRQ_TYPE_EDGE_RISING    	0x00000001
#define IRQ_TYPE_EDGE_FALLING  		0x00000002
#define IRQ_TYPE_EDGE_BOTH                           \
    (IRQ_TYPE_EDGE_FALLING | IRQ_TYPE_EDGE_RISING)
#define IRQ_TYPE_LEVEL_HIGH     	0x00000004
#define IRQ_TYPE_LEVEL_LOW      	0x00000008
#define IRQ_TYPE_LEVEL_MASK                          \
    (IRQ_TYPE_LEVEL_LOW | IRQ_TYPE_LEVEL_HIGH)
#define IRQ_TYPE_SENSE_MASK     	0x0000000f

#define IRQ_TYPE_INVALID        	0x00000010

void gic_init(void);
void gic_secondary_init(void);

#endif
