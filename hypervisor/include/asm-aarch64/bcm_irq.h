#ifndef __BCM_IRQ_H__
#define __BCM_IRQ_H__

#define LOCAL_CONTROL			0x000
#define LOCAL_PRESCALER			0x008

/*
 * The low 2 bits identify the CPU that the GPU IRQ goes to, and the
 * next 2 bits identify the CPU that the GPU FIQ goes to.
 */
#define LOCAL_GPU_ROUTING		0x00c
/* When setting bits 0-3, enables PMU interrupts on that CPU. */
#define LOCAL_PM_ROUTING_SET		0x010
/* When setting bits 0-3, disables PMU interrupts on that CPU. */
#define LOCAL_PM_ROUTING_CLR		0x014
/*
 * The low 4 bits of this are the CPU's timer IRQ enables, and the
 * next 4 bits are the CPU's timer FIQ enables (which override the IRQ
 * bits).
 */
#define LOCAL_TIMER_INT_CONTROL0	0x040
#define LOCAL_TIMER_INT_CONTROL3	0x04c
/*
 * The low 4 bits of this are the CPU's per-mailbox IRQ enables, and
 * the next 4 bits are the CPU's per-mailbox FIQ enables (which
 * override the IRQ bits).
 */
#define LOCAL_MAILBOX_INT_CONTROL0	0x050
#define LOCAL_MAILBOX_INT_CONTROL3	0x05c
/*
 * The CPU's interrupt status register.  Bits are defined by the the
 * LOCAL_IRQ_* bits below.
 */
#define LOCAL_IRQ_PENDING0		0x060
/* Same status bits as above, but for FIQ. */
#define LOCAL_FIQ_PENDING0		0x070
/*
 * Mailbox write-to-set bits.  There are 16 mailboxes, 4 per CPU, and
 * these bits are organized by mailbox number and then CPU number.  We
 * use mailbox 0 for IPIs.  The mailbox's interrupt is raised while
 * any bit is set.
 */
#define LOCAL_MAILBOX0_SET0		0x080
#define LOCAL_MAILBOX3_SET0		0x08c

#define LOCAL_MAILBOX_SET_START		0x80
#define LOCAL_MAILBOX_SET_END		0xbc

/* Mailbox write-to-clear bits. */
#define LOCAL_MAILBOX0_CLR0		0x0c0
#define LOCAL_MAILBOX3_CLR0		0x0cc

#define LOCAL_MAILBOX_CLR_START		0x0c0
#define LOCAL_MAILBOX_CLR_END		0x0fc

#define LOCAL_IRQ_CNTPSIRQ	0
#define LOCAL_IRQ_CNTPNSIRQ	1
#define LOCAL_IRQ_CNTHPIRQ	2
#define LOCAL_IRQ_CNTVIRQ	3
#define LOCAL_IRQ_MAILBOX0	4
#define LOCAL_IRQ_MAILBOX1	5
#define LOCAL_IRQ_MAILBOX2	6
#define LOCAL_IRQ_MAILBOX3	7
#define LOCAL_IRQ_GPU_FAST	8
#define LOCAL_IRQ_PMU_FAST	9
#define LAST_IRQ		LOCAL_IRQ_PMU_FAST

#define BCM2836_INC_BASE		0x40000000
#define BCM2835_INC_OFFSET		0x200

/* max support 16 cpus */
#define BCM2836_RELEASE_ADDR		0x40000180
#define BCM2836_RELEASE_OFFSET		0x180
#define BCM2836_RELEASE_OFFSET_END	0x1ff

#define BCM2835_IRQ_BASIC_PENDING	0x200
#define BCM2835_IRQ_PENDING1		0x204
#define BCM2835_IRQ_PENDING2		0x208
#define BCM2835_FIQ_CRTL		0x20c
#define BCM2835_IRQ_ENABLE1		0x210
#define BCM2835_IRQ_ENABLE2		0x214
#define BCM2835_IRQ_BASIC_ENABLE	0x218
#define BCM2835_IRQ_DISABLE1		0x21c
#define BCM2835_IRQ_DISABLE2		0x220
#define BCM2835_IRQ_DISABLE_BASIC	0x224
#define BCM2835_IRQ_ACK			0x228
#define BCM2836_IRQ_ACK			0x22c

/* Put the bank and irq (32 bits) into the hwirq */
#define MAKE_HWIRQ(b, n)	(((b + 32) << 5) | (n))
#define HWIRQ_BANK(i)		((i >> 5) - 1)
#define HWIRQ_BIT(i)		BIT(i & 0x1f)

#define NR_IRQS_BANK0		8
#define BANK0_HWIRQ_MASK	0xff
/* Shortcuts can't be disabled so any unknown new ones need to be masked */
#define SHORTCUT1_MASK		0x00007c00
#define SHORTCUT2_MASK		0x001f8000
#define SHORTCUT_SHIFT		10
#define BANK1_HWIRQ		BIT(8)
#define BANK2_HWIRQ		BIT(9)
#define BANK0_VALID_MASK	(BANK0_HWIRQ_MASK | BANK1_HWIRQ | BANK2_HWIRQ \
					| SHORTCUT1_MASK | SHORTCUT2_MASK)

#undef ARM_LOCAL_GPU_INT_ROUTING
#define ARM_LOCAL_GPU_INT_ROUTING 0x0c

#define REG_FIQ_CONTROL		0x0c
#define REG_FIQ_ENABLE		0x80
#define REG_FIQ_DISABLE		0

#define NR_BANKS		3
#define IRQS_PER_BANK		32
#define NUMBER_IRQS		MAKE_HWIRQ(NR_BANKS, 0)
#undef FIQ_START
#define FIQ_START		(NR_IRQS_BANK0 + MAKE_HWIRQ(NR_BANKS - 1, 0))

/*********
The interrupt sources are as follows:

Bank 0:
0: ARM_TIMER
1: ARM_MAILBOX
2: ARM_DOORBELL_0
3: ARM_DOORBELL_1
4: VPU0_HALTED
5: VPU1_HALTED
6: ILLEGAL_TYPE0
7: ILLEGAL_TYPE1
8 - 20 : used for gpu
21 - 31 : used for vm0 VMCS

Bank 1:
0: TIMER0
1: TIMER1
2: TIMER2
3: TIMER3
4: CODEC0
5: CODEC1
6: CODEC2
7: VC_JPEG
8: ISP
9: VC_USB
10: VC_3D
11: TRANSPOSER
12: MULTICORESYNC0
13: MULTICORESYNC1
14: MULTICORESYNC2
15: MULTICORESYNC3
16: DMA0
17: DMA1
18: VC_DMA2
19: VC_DMA3
20: DMA4
21: DMA5
22: DMA6
23: DMA7
24: DMA8
25: DMA9
26: DMA10
27: DMA11-14 - shared interrupt for DMA 11 to 14
28: DMAALL - triggers on all dma interrupts (including chanel 15)
29: AUX
30: ARM
31: VPUDMA

Bank 2:
0: HOSTPORT
1: VIDEOSCALER
2: CCP2TX
3: SDC
4: DSI0
5: AVE
6: CAM0
7: CAM1
8: HDMI0
9: HDMI1
10: PIXELVALVE1
11: I2CSPISLV
12: DSI1
13: PWA0
14: PWA1
15: CPR
16: SMI
17: GPIO0
18: GPIO1
19: GPIO2
20: GPIO3
21: VC_I2C
22: VC_SPI
23: VC_I2SPCM
24: VC_SDIO
25: VC_UART
26: SLIMBUS
27: VEC
28: CPG
29: RNG
30: VC_ARASANSDIO
31: AVSPMON

**********/

int bcm_virq_send_virq(struct vcpu *vcpu, uint32_t virq);

#endif
