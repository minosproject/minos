#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/virtio.h>
#include <linux/virtio_console.h>
#include <linux/virtio_ids.h>
#include <linux/virtio_mmio.h>
#include <linux/virtio_console.h>
#include <linux/serial_core.h>
#include <linux/console.h>

static void virtio_console_early_write(struct console *con,
		const char *s, unsigned n)
{
	int i, nr;
	uint32_t tmp;
	uint32_t write_ch = 0;
	static uint32_t init_done = 0;
	static uint32_t can_write = 0;
	struct earlycon_device *dev = con->data;
	void *early_base;
	struct virtio_console_config *p;

	early_base = dev->port.membase;
	p = early_base + VIRTIO_MMIO_CONFIG;
	if (!init_done) {
		tmp = readl_relaxed(early_base + VIRTIO_MMIO_DEVICE_ID);
		if (tmp != VIRTIO_ID_CONSOLE) {
			init_done = 1;
			return;
		}

		writel_relaxed(0, early_base + VIRTIO_MMIO_DEVICE_FEATURES_SEL);
		tmp = readl_relaxed(early_base + VIRTIO_MMIO_DEVICE_FEATURES);
		if (!(tmp & (1 << VIRTIO_CONSOLE_F_EMERG_WRITE))) {
			init_done = 1;
			return;
		}

		init_done = 1;
		can_write = 1;
	}

	if (can_write) {
		while (n > 0) {
			nr = n > 4 ? 4 : n;
			for (i = 0; i < nr; i++, s++)
				write_ch |= (*s) << (i * 8);

			writel_relaxed(write_ch, &p->emerg_wr);
			n -= nr;
			write_ch = 0;
		}
	}
}

static int __init virtio_early_console_setup(struct earlycon_device *device, const char *opt)
{
	if (!device->port.membase)
		return -ENODEV;

	device->con->write = virtio_console_early_write;
	return 0;
}
EARLYCON_DECLARE(virtio_console, virtio_early_console_setup);
OF_EARLYCON_DECLARE(virtio_console, "virtio_console",
		virtio_early_console_setup);
