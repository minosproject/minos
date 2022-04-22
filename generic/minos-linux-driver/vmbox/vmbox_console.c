/*
 * Copyright (C) 2019 Min Le (lemin9538@gmail.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/console.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/irq.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/serial_core.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include "../../tty/hvc/hvc_console.h"

#include <asm/io.h>
#include "vmbox_bus.h"
#include "../minos.h"

#define VMBOX_HVC_COOLIE	0xeeffdd00
#define VMBOX_HVC_NR		8

#define VMBOX_HVC_STAT_CLOSED	0x0
#define VMBOX_HVC_STAT_OPENED	0x1

#define VMBOX_HVC_EVENT_HANGUP	VMBOX_IPC_USER_EVENT(0)
#define VMBOX_HVC_EVENT_RX	VMBOX_IPC_USER_EVENT(1)
#define VMBOX_HVC_EVENT_TX_FULL	VMBOX_IPC_USER_EVENT(2)

static struct vmbox_console *hvc_consoles[VMBOX_HVC_NR];
static DEFINE_SPINLOCK(vmbox_console_lock);
static int hvc_index;

#define BUF_0_SIZE	4096
#define BUF_1_SIZE	2048
#define BUF_SIZE	8192

#define HVC_VMBOX_CONSOLE	0
#define HVC_DEBUG_CONSOLE	1

struct vmbox_console {
	int id;
	int type;
	uint32_t irq;
	struct vmbox_device *vdev;
	struct hvc_struct *hvc;
	int vetrmno;
	int backend;
	int otherside_state;
	struct vm_ring *tx;
	struct vm_ring *rx;
};

static inline struct
vmbox_console *vtermno_to_vmbox_console(uint32_t vtermno)
{
	if ((vtermno & 0xff) >= VMBOX_HVC_NR)
		return NULL;

	return hvc_consoles[vtermno & 0xff];
}

static int vmbox_hvc_read_console(uint32_t vtermno, char *buf, int count)
{
	struct vmbox_console *vc = vtermno_to_vmbox_console(vtermno);
	struct vm_ring *ring = vc->rx;
	uint32_t ridx, widx,  recv = 0;
	char *buffer;

	ridx = ring->ridx;
	widx = ring->widx;
	buffer = ring->buf;

	mb();
	BUG_ON((widx - ridx) > ring->size);

	/* index overflow ? */
	while ((ridx != widx) && (recv < count))
		buf[recv++] = buffer[VM_RING_IDX(ridx++, ring->size)];

	mb();
	ring->ridx = ridx;

	return recv;
}

static int vmbox_hvc_write_console(uint32_t vtermno, const char *buf, int count)
{
	struct vmbox_console *vc = vtermno_to_vmbox_console(vtermno);
	struct vm_ring *ring = vc->tx;
	uint32_t ridx, widx, send = 0;
	char *buffer;
	int len = count;

	while (count) {
again:
		ridx = ring->ridx;
		widx = ring->widx;
		buffer = ring->buf;
		mb();

		/*
		 * when overflow happend, if the otherside is not opened
		 */
		if (((widx - ridx) == ring->size)) {
			if (vc->otherside_state != VMBOX_DEV_STAT_OPENED) {
				ridx += count;
				ring->ridx = ridx;
				wmb();
			} else {
				/* here wait for other side process the data */
				vmbox_device_vring_event(vc->vdev);
				hvc_sched_out();
				goto again;
			}
		}

		while ((send < count) && (widx - ridx) < ring->size)
			buffer[VM_RING_IDX(widx++, ring->size)] = buf[send++];

		mb();
		ring->widx = widx;

		count -= send;
		buf += send;

		if (send && vc->vdev && vc->otherside_state == VMBOX_DEV_STAT_OPENED)
			vmbox_device_vring_event(vc->vdev);
		send = 0;
	}

	return len;
}

static int vmbox_hvc_notifier_add(struct hvc_struct *hp, int irq)
{
	int ret;
	struct vmbox_console *vc = vtermno_to_vmbox_console(hp->vtermno);

	if (!vc)
		return -ENOENT;

	if (!vc->vdev)
		return 0;

	ret = notifier_add_irq(hp, irq);
	if (ret)
		return ret;

	/* indicate the other side that I have opened */
	vmbox_device_state_change(vc->vdev, VMBOX_DEV_STAT_OPENED);

	return 0;
}

static void vmbox_hvc_notifier_del(struct hvc_struct *hp, int irq)
{
	struct vmbox_console *vc = vtermno_to_vmbox_console(hp->vtermno);

	if (!vc || !vc->vdev)
		return;

	/* indicate the other side that I have closed */
	vmbox_device_state_change(vc->vdev, VMBOX_DEV_STAT_CLOSED);
	notifier_del_irq(hp, irq);
}

static void vmbox_hvc_notifier_hangup(struct hvc_struct *hp, int irq)
{
	struct vmbox_console *vc = vtermno_to_vmbox_console(hp->vtermno);

	if (!vc || !vc->vdev)
		return;

	/* indicate the other side that I have hangup */
	vmbox_device_state_change(vc->vdev, VMBOX_DEV_STAT_CLOSED);
	notifier_del_irq(hp, irq);
}

static const struct hv_ops vmbox_hvc_ops = {
	.get_chars = vmbox_hvc_read_console,
	.put_chars = vmbox_hvc_write_console,
	.notifier_add = vmbox_hvc_notifier_add,
	.notifier_del = vmbox_hvc_notifier_del,
	.notifier_hangup = vmbox_hvc_notifier_hangup,
};

static void inline
vmbox_vm_ring_setup(struct vmbox_console *vc, void *base, int backend)
{
	int header_size = sizeof(struct vm_ring);

	if (backend) {
		vc->tx = (struct vm_ring *)base;
		vc->rx = (struct vm_ring *)(base + header_size + BUF_0_SIZE);
	} else {
		vc->rx = (struct vm_ring *)base;
		vc->tx = (struct vm_ring *)(base + header_size + BUF_0_SIZE);
	}
}

static int vmbox_hvc_vring_init(struct vmbox_console *vc)
{
	void *base;
	struct vmbox_device *vdev = vc->vdev;

	/*
	 * rxbuf - at least 2048
	 */
	if (vdev->vring_mem_size < 8192)
		return -ENOSPC;

	if (vc->tx && vc->backend)
		vdev->vring_va = ((void *)vc->tx) - VMBOX_IPC_ALL_ENTRY_SIZE;
	else
		pr_err("vmbox console front-end\n");

	base = vmbox_device_remap(vdev);
	if (!base)
		return -ENOMEM;

	if (!vc->tx || !vc->rx)
		vmbox_vm_ring_setup(vc, base, vc->backend);

	return 0;
}

static int vm0_read_console(uint32_t vtermno, char *buf, int count)
{
       return 0;
}

static int vm0_write_console(uint32_t vtermno, const char *buf, int count)
{
       return count;
}

static const struct hv_ops vm0_hvc_ops = {
       .get_chars = vm0_read_console,
       .put_chars = vm0_write_console,
};

static int vm_debug_hvc_notifier_add(struct hvc_struct *hp, int irq)
{
	notifier_add_irq(hp, irq);
	minos_hvc0(HVC_DC_OPEN);

	return 0;
}

static void vm_debug_hvc_notifier_del(struct hvc_struct *hp, int irq)
{
	notifier_del_irq(hp, irq);
	minos_hvc0(HVC_DC_CLOSE);
}

static void vm_debug_hvc_notifier_hangup(struct hvc_struct *hp, int irq)
{
	notifier_del_irq(hp, irq);
	minos_hvc0(HVC_DC_CLOSE);
}

static int vm_debug_read_console(uint32_t vtermno, char *buf, int count)
{
	struct vmbox_console *vc = hvc_consoles[0];
	struct vm_ring *ring = vc->rx;
	uint32_t ridx, widx,  recv = 0;
	char *buffer;

	if (!vc || vc->type != HVC_DEBUG_CONSOLE)
		return 0;

	ridx = ring->ridx;
	widx = ring->widx;
	buffer = ring->buf;
	mb();

	BUG_ON((widx - ridx) > ring->size);

	/* index overflow ? */
	while ((ridx != widx) && (recv < count))
		buf[recv++] = buffer[VM_RING_IDX(ridx++, ring->size)];

	mb();
	ring->ridx = ridx;

	return recv;
}

static int vm_debug_write_console(uint32_t vtermno, const char *buf, int count)
{
	int send = 0;
	struct vm_ring *tx;
	uint32_t widx;
	struct vmbox_console *vc = hvc_consoles[0];

	if (!vc || vc->type != HVC_DEBUG_CONSOLE)
		return count;

	tx = vc->tx;
	widx = tx->widx;
	mb();

	BUG_ON((widx - tx->ridx) >= tx->size);

	while(send < count)
		vc->tx->buf[VM_RING_IDX(widx++, tx->size)] = buf[send++];

	mb();
	tx->widx = widx;

	minos_hvc0(HVC_DC_WRITE);

	return count;
}

static const struct hv_ops vm_debug_hvc_ops = {
	.get_chars = vm_debug_read_console,
	.put_chars = vm_debug_write_console,
	.notifier_add = vm_debug_hvc_notifier_add,
	.notifier_del = vm_debug_hvc_notifier_del,
	.notifier_hangup = vm_debug_hvc_notifier_hangup,
};

static int vmbox_hvc_probe(struct vmbox_device *vdev)
{
	struct hvc_struct *hp;
	struct vmbox_console *vc;
	int ishost = vm_is_host_vm();

	/*
	 * if the vm is 0 and there is no debug console
	 * then register a fake hvc console
	 */
	pr_info("vmbox_hvc_probe : [%s] hvc_index[%d]\n",
			ishost ? "Host" : "Natvie", hvc_index);
	if (ishost && hvc_index == 0) {
		pr_info("register a fake hvc for vm0\n");
		hvc_alloc(VMBOX_HVC_COOLIE + 0xff, 0, &vm0_hvc_ops, 16);
		hvc_index++;
	}

	/*
	 * if the hvc in this VM is a forentend this hvc console
	 * will register at console_init stage, so do not realloc
	 * this hvc console at this stage
	 */
	if (hvc_consoles[hvc_index])
		vc = hvc_consoles[hvc_index];
	else {
		vc = kzalloc(sizeof(*vc), GFP_KERNEL);
		if (!vc)
			return -ENOMEM;
		vc->backend = vmbox_device_is_backend(vdev);

		spin_lock(&vmbox_console_lock);
		vc->id = hvc_index++;
		hvc_consoles[vc->id] = vc;
		spin_unlock(&vmbox_console_lock);

		vc->vetrmno = VMBOX_HVC_COOLIE + vc->id;
	}

	if (vc->type == HVC_DEBUG_CONSOLE)
		panic("debug console should not register here\n");

	vc->vdev = vdev;

	/* init the vmbox device and the console */
	vmbox_device_init(vdev, VMBOX_F_NO_VIRTQ);
	vmbox_set_drvdata(vdev, vc);

	if (vmbox_hvc_vring_init(vc)) {
		kfree(vc);
		return -ENOMEM;
	}

	vc->otherside_state = vmbox_device_otherside_state(vdev);
	vc->irq = vdev->vring_irq;

	hp = hvc_alloc(vc->vetrmno, vc->irq, &vmbox_hvc_ops, 256);
	if (IS_ERR(hp)) {
		kfree(vc);
		return PTR_ERR(hp);
	}

	vmbox_device_online(vdev);

	return 0;
}

static void vmbox_hvc_remove(struct vmbox_device *vdev)
{
	struct vmbox_console *vc = vmbox_get_drvdata(vdev);

	vmbox_device_offline(vdev);
	hvc_consoles[vc->id] = NULL;
	vmbox_device_unmap(vdev);
	kfree(vc);
}

static int vmbox_hvc_evt_handler(struct vmbox_device *vdev,
		uint32_t event)
{
	switch (event) {
	case VMBOX_HVC_EVENT_RX:
		break;
	case VMBOX_HVC_EVENT_TX_FULL:
		pr_err("vmbox console tx full\n");
		// vmbox_hvc_flush_data(vc, 16);
		break;
	default:
		break;
	}

	return 0;
}

static int vmbox_hvc_state_change(struct vmbox_device *vdev, int state)
{
	struct vmbox_console *vc = vmbox_get_drvdata(vdev);

	switch (state) {
	case VMBOX_DEV_STAT_OPENED:
		break;
	case VMBOX_DEV_STAT_CLOSED:
		break;
	default:
		break;
	}

	vc->otherside_state = state;

	return 0;
}

static struct vmbox_device_id vmbox_hvc_ids[] = {
	{0x3420, VMBOX_ANY_VENDOR_ID},
	{0x3421, VMBOX_ANY_VENDOR_ID},
	{}
};

static struct vmbox_driver vmbox_console_drv = {
	.id_table = vmbox_hvc_ids,
	.probe = vmbox_hvc_probe,
	.remove = vmbox_hvc_remove,
	.otherside_evt_handler = vmbox_hvc_evt_handler,
	.otherside_state_change = vmbox_hvc_state_change,
	.driver = {
		.name = "vmbox-console",
	},
};

static int __init vmbox_console_init(void)
{
	struct hvc_struct *hp;
	struct vmbox_console *vc;

	vc = hvc_consoles[0];
	if (vc && (vc->type == HVC_DEBUG_CONSOLE)) {
		vc->irq = get_dynamic_virq(vc->irq);

		hp = hvc_alloc(vc->vetrmno,
			vc->irq, &vm_debug_hvc_ops, 256);
		if (IS_ERR(hp))
			pr_err("register vm debug console failed\n");
	}

	return vmbox_register_driver(&vmbox_console_drv);
}

static void __exit vmbox_console_exit(void)
{
	vmbox_unregister_driver(&vmbox_console_drv);
}

module_init(vmbox_console_init);
module_exit(vmbox_console_exit);
MODULE_LICENSE("GPL");

static int __init vm_init_vmbox_console(void)
{
	struct device_node *node;
	struct resource reg;
	struct vmbox_console *vc;
	void *console_ring;

	pr_info("vmbox hvc console init for backend\n");

	/*
	 * to detected whether there is a vmbox hvc froent
	 * device, if yes, register it, the hvc index will start
	 * at 0, otherwise, it means this vm is HVM, the hvc console
	 * index whill start at 1, since systemd will automaticlly
	 * open hvc0
	 */
	node = of_find_compatible_node(NULL, NULL, "minos,hvc-be");
	if (!node) {
		pr_err("can not find the hvc console device\n");
		return -ENOENT;
	}

	if (of_address_to_resource(node, 0, &reg)) {
		pr_err("can not get hvc address\n");
		return -ENOMEM;
	}

	console_ring = ioremap_wc(reg.start, resource_size(&reg));
	if (!console_ring)
		return -ENOMEM;

	vc = kzalloc(sizeof(*vc), GFP_KERNEL);
	if (!vc)
		return -ENOMEM;

	vmbox_vm_ring_setup(vc, console_ring +
			VMBOX_IPC_ALL_ENTRY_SIZE, 1);

	vc->vetrmno = VMBOX_HVC_COOLIE + hvc_index;
	vc->backend = 1;
	vc->id = hvc_index;
	hvc_consoles[hvc_index] = vc;
	wmb();

	return 0;
}

static struct vmbox_console *create_vm_debug_console(void)
{
	uint32_t id;
	uint32_t irq;
	unsigned long ring_addr;
	void *base;
	struct vmbox_console *vc;

	id = minos_hvc0(HVC_DC_GET_STAT);
	if ((id & 0xffff0000) != 0xabcd0000)
		return NULL;

	irq = minos_hvc0(HVC_DC_GET_IRQ);
	ring_addr = minos_hvc0(HVC_DC_GET_RING);
	if (!irq || !ring_addr)
		return NULL;

	base = ioremap_wc(ring_addr, BUF_SIZE);
	if (!base)
		return NULL;

	vc = kzalloc(sizeof(struct vmbox_console), GFP_KERNEL);
	if (!vc)
		return NULL;

	vc->rx = (struct vm_ring *)base;
	vc->tx = (struct vm_ring *)(base +
			sizeof(struct vm_ring) + BUF_1_SIZE);

	/*
	 * here hvc_index need plus 1, the hvc0 is debug
	 * console, then the vmbox console will start with
	 * the index 1
	 */
	vc->vetrmno = VMBOX_HVC_COOLIE + hvc_index;
	vc->id = hvc_index++;
	vc->irq = irq;
	vc->type = HVC_DEBUG_CONSOLE;
	hvc_consoles[vc->id] = vc;

	return vc;
}

static void debug_console_write(struct console *con, const char *s, unsigned n)
{
	int send = 0;
	struct vmbox_console *vc = hvc_consoles[0];
	struct vm_ring *tx;

	if (vc == NULL)
		return;

	tx = vc->tx;

	if ((tx->widx - tx->ridx) == tx->size)
		tx->ridx += n;

	while (send < n)
		tx->buf[VM_RING_IDX(tx->widx++, tx->size)] = s[send++];

	mb();
	minos_hvc0(HVC_DC_WRITE);
}

static int __init debug_console_setup(struct earlycon_device *device,
		const char *opt)
{
	struct vmbox_console *vc = hvc_consoles[0];

	if (vc == NULL)
		vc = create_vm_debug_console();

	device->con->write = debug_console_write;
	return 0;
}
EARLYCON_DECLARE(vm_debug_con, debug_console_setup);
OF_EARLYCON_DECLARE(vm_debug_con, "dbcon", debug_console_setup);

static int __init vm_init_debug_console(void)
{
	struct vmbox_console *vc;

	vc = hvc_consoles[0];
	if (!vc) {
		vc = create_vm_debug_console();
		if (!vc)
			return -ENODEV;
	}

	pr_info("register vm debug console\n");
	hvc_instantiate(VMBOX_HVC_COOLIE + vc->id,
			vc->id, &vm_debug_hvc_ops);

	return 0;
}

/*
 * for native vm there two type hvc
 * 0 - is the debug console which is handled by hypervisor
 * 1 - from 1 its vmbox console
 */
static int vm_console_init(void)
{
	/*
	 * init the debug console then init the vmbox
	 * console
	 */
	vm_init_debug_console();
	vm_init_vmbox_console();

	return 0;
}
console_initcall(vm_console_init);
