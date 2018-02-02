struct gic_memory {
	unsigned char gicv3[0x100000];
};

struct gic_memory __attribute__((section(".gicd"))) __gicd;
