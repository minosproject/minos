/*
 * Copyright (C) 2018 - 2019 Min Le (lemin9538@gmail.com)
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

#include <minos/minos.h>
#include <minos/mm.h>
#include <libfdt/libfdt.h>
#include <minos/of.h>
#include <minos/irq.h>

#define OF_MAX_DEEPTH	5

int fdt_n_size_cells(void *dtb, int node)
{
	fdt32_t *v;
	int parent, child = node;

	parent = fdt_parent_offset(dtb, child);

	do {
		if (parent >= 0)
			child = parent;
		v = (fdt32_t *)fdt_getprop(dtb, child, "#size-cells", NULL);
		if (v)
			return fdt32_to_cpu(*v);

		parent = fdt_parent_offset(dtb, child);
	} while (parent >= 0);

	return 2;
}

int fdt_n_addr_cells(void *dtb, int node)
{
	fdt32_t *v;
	int parent, child = node;

	parent = fdt_parent_offset(dtb, child);

	do {
		if (parent >= 0)
			child = parent;
		v = (fdt32_t *)fdt_getprop(dtb, child, "#address-cells", NULL);
		if (v)
			return fdt32_to_cpu(*v);

		parent = fdt_parent_offset(dtb, child);
	} while (parent >= 0);

	return 2;
}

static inline void *__of_getprop(void *dtb, int node,
		char *attr, int *len)
{
	const void *data;

	if (!dtb || node <= 0 || !attr)
		return NULL;

	data = fdt_getprop(dtb, node, attr, len);
	if (!data || (*len <= 0))
		return NULL;

	return (void *)data;
}

void *of_getprop(struct device_node *node, char *attr, int *len)
{
	return __of_getprop(node->data, node->offset, attr, len);
}

static int __of_get_node_by_name(void *data, int pnode,
		char *str, int deepth)
{
	const char *name;
	int node = -1, child, len;

	if (OF_MAX_DEEPTH >= 10)
		return -ENOENT;

	fdt_for_each_subnode(node, data, pnode) {
		if (NULL == (name = fdt_get_name(data, node, &len)))
			continue;
		if (len < 0)
			continue;

		if (strncmp(name, str, strlen(str)) == 0)
			return node;
		else {
			child = __of_get_node_by_name(data, node,
					str, deepth + 1);
			if (child > 0)
				return child;
		}
	}

	return -ENOENT;
}

int of_get_node_by_name(void *data, int pnode, char *str)
{
	if (!data || (pnode < 0) || !str)
		return -EINVAL;

	return __of_get_node_by_name(data, pnode, str, 0);
}

const char *__of_get_compatible(void *dtb, int node)
{
	const void *data;
	int len;

	data = fdt_getprop(dtb, node, "compatible", &len);
	if (!data || len == 0)
		return NULL;

	return (const char *)data;
}

int __of_get_bool(void *dtb, int node, char *attr)
{
	int len;
	const struct fdt_property *prop;

	prop = fdt_get_property(dtb, node, attr, &len);
	return (prop != NULL);
}

int of_get_bool(struct device_node *node, char *attr)
{
	return __of_get_bool(node->data, node->offset, attr);
}

int __of_get_string(void *dtb, int node, char *attr, char *str, int len)
{
	char *s;
	int length;

	memset(str, 0, len);
	s = (char *)__of_getprop(dtb, node, attr, &length);
	if (!s || !str || (length == 0))
		return -EINVAL;

	length = MIN(len - 1, length);
	strncpy(str, s, length);

	return length;
}

char *of_get_cmdline(void *dtb)
{
	int node, len;
	const void *data = NULL;

	node = fdt_path_offset(dtb, "/chosen");
	if (node <= 0)
		return NULL;

	data = fdt_getprop(dtb, node, "bootargs", &len);
	return (char *)data;
}

int __of_get_u16_array(void *dtb, int node, char *attr,
		uint16_t *array, int len)
{
	fdt16_t *val;
	int length, i;

	memset(array, 0, sizeof(uint16_t) * len);
	val = (fdt16_t *)__of_getprop(dtb, node, attr, &length);
	if (!val)
		return -EINVAL;

	if ((length % sizeof(uint16_t)) != 0) {
		pr_err("node is not a u32 array %d\n", length);
		return -EINVAL;
	}

	length = length / sizeof(fdt16_t);
	length = MIN(len, length);

	for (i = 0; i < length; i++)
		*array++ = fdt16_to_cpu(val[i]);

	return length;
}

int __of_get_u32_array(void *dtb, int node, char *attr,
		uint32_t *array, int len)
{
	fdt32_t *val;
	int length, i;

	memset(array, 0, sizeof(uint32_t) * len);
	val = (fdt32_t *)__of_getprop(dtb, node, attr, &length);
	if (!val)
		return -EINVAL;

	if ((length % sizeof(fdt32_t)) != 0) {
		pr_err("node is not a u32 array %d\n", length);
		return -EINVAL;
	}

	length = length / sizeof(fdt32_t);
	length = MIN(len, length);

	for (i = 0; i < length; i++)
		*array++ = fdt32_to_cpu(val[i]);

	return length;
}

int __of_get_u64_array(void *dtb, int node, char *attr,
		uint64_t *array, int len)
{
	fdt64_t *val;
	int length, i;

	memset(array, 0, sizeof(uint64_t) * len);
	val = (fdt64_t *)__of_getprop(dtb, node, attr, &length);
	if (!val) {
		pr_err("can not get %s\n", attr);
		return -EINVAL;
	}

	if ((length % sizeof(fdt64_t)) != 0) {
		pr_err("node is not a u64 array %d\n", length);
		return -EINVAL;
	}

	length = length / sizeof(uint64_t);
	length = MIN(len, length);

	for (i = 0; i < length; i++)
		*array++ = fdt64_to_cpu(val[i]);

	return length;
}

static inline struct device_node *alloc_device_node(void)
{
	struct device_node *node;

	node = zalloc(sizeof(struct device_node));
	if (!node) {
		pr_warn("%s no enough memory\n", __func__);
		return NULL;
	}

	return node;
}

static int of_parse_dt_class(struct device_node *node)
{
	int ret;
	char type[64];

	ret = __of_get_string(node->data, node->offset,
			"device_type", type, 64);
	if (ret > 0) {
		if (strcmp(type, "cpu") == 0)
			node->class = DT_CLASS_CPU;
		else if (strcmp(type, "memory") == 0)
			node->class = DT_CLASS_MEMORY;
		else if (strcmp(type, "pci") == 0)
			node->class = DT_CLASS_PCI_BUS;
		else if (strcmp(type, "virtual_machine") == 0)
			node->class = DT_CLASS_VM;
		else
			node->class = DT_CLASS_OTHER;
	} else {
		ret = __of_get_bool(node->data, node->offset,
				"interrupt-controller");
		if (ret) {
			node->class = DT_CLASS_IRQCHIP;
			return 0;
		}

		if (strcmp(node->name, "timer") == 0) {
			node->class = DT_CLASS_TIMER;
			return 0;
		}

		if (!fdt_node_check_compatible(node->data,
				node->offset, "simple-bus")) {
			node->class = DT_CLASS_SIMPLE_BUS;
			return 0;
		}

		ret = __of_get_bool(node->data, node->offset,
				"virtual_device");
		if (ret) {
			node->class = DT_CLASS_VDEV;
			return 0;
		}

		switch (node->parent->class) {
		case DT_CLASS_IRQCHIP:
			node->class = DT_CLASS_PDEV;
			break;
		case DT_CLASS_CPU:
		case DT_CLASS_PDEV:
		case DT_CLASS_VDEV:
			node->class = node->parent->class;
			return 0;
		default:
			break;;
		}

		if (node->compatible)
			node->class = DT_CLASS_PDEV;
		else
			node->class = DT_CLASS_OTHER;
	}

	return 0;
}

int of_device_match(struct device_node *node, char **comp)
{
	if (!node || !comp)
		return -EINVAL;

	while (*comp != NULL) {
		if (!fdt_node_check_compatible(node->data,
				node->offset, *comp))
			return 1;

		comp++;
	}

	return 0;
}

static int __of_parse_device_node(struct device_node *root,
		struct device_node *pnode)
{
	int child, index = 0;
	struct device_node *node, *prev;
	void *data = pnode->data;

	if (!pnode)
		return -EINVAL;

	fdt_for_each_subnode(child, data, pnode->offset) {
		node = alloc_device_node();
		if (!node)
			return -ENOMEM;

		node->name = fdt_get_name(data, child, NULL);
		node->compatible = __of_get_compatible(data, child);
		node->offset = child;
		node->data = data;
		node->parent = pnode;
		node->flags |= DEVICE_NODE_F_OF;

		/* udpate the child and the sibling */
		if (index == 0) {
			pnode->child = node;
			index = 1;
		} else {
			prev->sibling = node;
		}

		prev = node;
		node->next = root->next;
		root->next = node;
		of_parse_dt_class(node);
		__of_parse_device_node(root, node);
	}

	return 0;
}

/* must pass a root device node to this function */
static void *__iterate_device_node(struct device_node *node,
		of_iterate_fn func, void *arg, int loop)
{
	struct device_node *child, *sibling, *n;

	if (!node)
		return NULL;

	child = node->child;
	n = func(node, arg);
	if (n && !loop)
		return n;

	while (child) {
		sibling = child->sibling;
		n = __iterate_device_node(child, func, arg, loop);
		if (n && !loop)
			return n;
		child = sibling;
	}

	return NULL;
}

void *of_iterate_all_node_loop(struct device_node *node,
		of_iterate_fn func, void *arg)
{
	if (!device_node_is_root(node)) {
		pr_err("root_device_node is not root\n");
		return NULL;
	}

	return __iterate_device_node(node, func, arg, 1);
}

void *of_iterate_all_node(struct device_node *node,
		of_iterate_fn func, void *arg)
{
	if (!device_node_is_root(node)) {
		pr_err("root_device_node is not root\n");
		return NULL;
	}

	return __iterate_device_node(node, func, arg, 0);
}

static void *find_node_by_compatible(struct device_node *node, void *comp)
{
	char **str = (char **)comp;

	if (of_device_match(node, str))
		return node;

	return NULL;
}

static void *find_node_by_name(struct device_node *node, void *name)
{
	if (node->name && !(strcmp(node->name, (char *)name)))
		return node;

	return NULL;
}

struct device_node *
of_find_node_by_compatible(struct device_node *root, char **comp)
{
	return (struct device_node *)__iterate_device_node(root,
			find_node_by_compatible, (void *)comp, 0);
}

struct device_node *
of_find_node_by_name(struct device_node *root, char *name)
{
	return (struct device_node *)__iterate_device_node(root,
			find_node_by_name, (void *)name, 0);
}

int of_n_addr_cells(struct device_node *node)
{
	fdt32_t *ip;

	do {
		ip = (fdt32_t *)fdt_getprop(node->data, node->offset,
				"#address-cells", NULL);
		if (ip)
			return fdt32_to_cpu(*ip);

		if (node->parent)
			node = node->parent;
	} while (node);

	return 2;
}

int of_n_size_cells(struct device_node *node)
{
	fdt32_t *ip;

	do {
		ip = (fdt32_t *)fdt_getprop(node->data, node->offset,
				"#size-cells", NULL);
		if (ip)
			return fdt32_to_cpu(*ip);

		if (node->parent)
			node = node->parent;
	} while (node);

	return 1;
}

int of_get_phandle(struct device_node *node)
{
	const struct fdt_property *prop;
	int len;

	prop = fdt_get_property(node->data, node->offset,
			"interrupt-parent", &len);
	if (!prop)
		return -1;

	return fdt32_to_cpu(*(fdt32_t *)prop->data);
}

int of_n_interrupt_cells(struct device_node *node)
{
	int ret, len;
	uint32_t ni = 0;
	struct device_node *parent = node;
	uint32_t phandle;
	int offset;
	const struct fdt_property *prop;

	while (parent) {
		prop = fdt_get_property(parent->data, parent->offset,
				"interrupt-parent", &len);
		if (!prop || !prop->data)
			goto repeat;

		phandle = fdt32_to_cpu(*(fdt32_t *)prop->data);
		offset = fdt_node_offset_by_phandle(parent->data, phandle);
		if (offset <= 0)
			return 0;

		ret = __of_get_u32_array(parent->data, offset,
				"#interrupt-cells", &ni, 1);
		if (ret == 1)
			return ni;
repeat:
		parent = parent->parent;
	}

	return 0;
}

int of_n_addr_count(struct device_node *node)
{
	int na, ns;
	int len;
	const void *prop;

	na = of_n_addr_cells(node);
	ns = of_n_size_cells(node);

	prop = fdt_getprop(node->data, node->offset, "reg", &len);
	if (!prop)
		return 0;

	len = len / ((na + ns) * sizeof(fdt32_t));
	return len;
}

int of_data(void *data)
{
	return !fdt_check_header(data);
}

static inline u64 of_read_number(const fdt32_t *cell, int size)
{
	u64 r = 0;
	while (size--) {
		r = (r << 32) | fdt32_to_cpu(*cell);
		cell++;
	}
	return r;
}

static inline int of_check_counts(int na, int ns)
{
	return ((na) > 0 && (na) <= OF_MAX_ADDR_CELLS && (ns) > 0);
}

/* Callbacks for bus specific translators */
struct of_bus {
	const char *name;
	const char *addresses;
	void (*count_cells)(void *blob, int parentoffset,
			int *addrc, int *sizec);
	uint64_t (*map)(u32 *addr, const u32 *range,
			int na, int ns, int pna);
	int (*translate)(u32 *addr, u64 offset, int na);
};

/* Default translator (generic bus) */
static void of_bus_default_count_cells(void *blob,
		int parentoffset, int *addrc, int *sizec)
{
	const fdt32_t *prop;

	if (addrc) {
		prop = fdt_getprop(blob, parentoffset, "#address-cells", NULL);
		if (prop)
			*addrc = fdt32_to_cpu(*prop);
		else
			*addrc = 2;
	}

	if (sizec) {
		prop = fdt_getprop(blob, parentoffset, "#size-cells", NULL);
		if (prop)
			*sizec = fdt32_to_cpu(*prop);
		else
			*sizec = 1;
	}
}

static uint64_t of_bus_default_map(fdt32_t *addr,
		const fdt32_t *range,
		int na, int ns, int pna)
{
	uint64_t cp, s, da;

	cp = of_read_number(range, na);
	s  = of_read_number(range + na + pna, ns);
	da = of_read_number(addr, na);

	pr_debug("OF: default map, cp=0x%p, s=0x%p, da=0x%p\n", cp, s, da);

	if (da < cp || da >= (cp + s))
		return OF_BAD_ADDR;
	return da - cp;
}

static int of_bus_default_translate(u32 *addr, u64 offset, int na)
{
	uint64_t a = of_read_number(addr, na);
	memset(addr, 0, na * 4);
	a += offset;
	if (na > 1) {
		addr[na - 2] = a >> 32;
		addr[na - 2] = cpu_to_fdt32(addr[na - 2]);
	}
	addr[na - 1] = a & 0xffffffffu;
	addr[na - 1] = cpu_to_fdt32(addr[na - 1]);

	return 0;
}

/* Array of bus specific translators */
static struct of_bus of_busses[] = {
	/* Default */
	{
		.name = "default",
		.addresses = "reg",
		.count_cells = of_bus_default_count_cells,
		.map = of_bus_default_map,
		.translate = of_bus_default_translate,
	},
};

static int of_translate_one(void * blob, int parent, struct of_bus *bus,
			    struct of_bus *pbus, uint32_t *addr,
			    int na, int ns, int pna, const char *rprop)
{
	const uint32_t *ranges;
	int rlen;
	int rone;
	uint64_t offset = OF_BAD_ADDR;

	/* Normally, an absence of a "ranges" property means we are
	 * crossing a non-translatable boundary, and thus the addresses
	 * below the current not cannot be converted to CPU physical ones.
	 * Unfortunately, while this is very clear in the spec, it's not
	 * what Apple understood, and they do have things like /uni-n or
	 * /ht nodes with no "ranges" property and a lot of perfectly
	 * useable mapped devices below them. Thus we treat the absence of
	 * "ranges" as equivalent to an empty "ranges" property which means
	 * a 1:1 translation at that level. It's up to the caller not to try
	 * to translate addresses that aren't supposed to be translated in
	 * the first place. --BenH.
	 */
	ranges = (uint32_t *)fdt_getprop(blob, parent, rprop, &rlen);
	if (ranges == NULL || rlen == 0) {
		offset = of_read_number(addr, na);
		memset(addr, 0, pna * 4);
		pr_debug("OF: no ranges, 1:1 translation\n");
		goto finish;
	}

	pr_debug("OF: walking ranges...\n");

	/* Now walk through the ranges */
	rlen /= 4;
	rone = na + pna + ns;
	for (; rlen >= rone; rlen -= rone, ranges += rone) {
		offset = bus->map(addr, ranges, na, ns, pna);
		if (offset != OF_BAD_ADDR)
			break;
	}
	if (offset == OF_BAD_ADDR) {
		pr_debug("OF: not found !\n");
		return 1;
	}
	memcpy(addr, ranges + na, 4 * pna);

 finish:
	/* Translate it into parent bus space */
	return pbus->translate(addr, offset, pna);
}

/*
 * Translate an address from the device-tree into a CPU physical address,
 * this walks up the tree and applies the various bus mappings on the
 * way.
 *
 * Note: We consider that crossing any level with #size-cells == 0 to mean
 * that translation is impossible (that is we are not dealing with a value
 * that can be mapped to a cpu physical address). This is not really specified
 * that way, but this is traditionally the way IBM at least do things
 */
static uint64_t __of_translate_address(struct device_node *node,
		const fdt32_t *in_addr, const char *prop)
{
	int node_offset;
	struct device_node *parent;
	fdt32_t addr[OF_MAX_ADDR_CELLS];
	int na, ns, pna, pns;
	struct of_bus *bus, *pbus;
	uint64_t result = OF_BAD_ADDR;
	void *data;

	pr_debug("OF: ** translation for device %s **\n", node->name);

	/* Get parent & match bus type */
	parent = node->parent;
	data = node->data;
	if (parent == NULL)
		goto bail;
	bus = &of_busses[0];

	/* Cound address cells & copy address locally */
	bus->count_cells(data, parent->offset, &na, &ns);
	if (!of_check_counts(na, ns)) {
		pr_warn("%s: Bad cell count for %s\n", __func__, parent->name);
		goto bail;
	}
	memcpy(addr, in_addr, na * 4);

	pr_debug("OF: bus is %s (na=%d, ns=%d) on %s\n",
			bus->name, na, ns, parent->name);

	/* Translate */
	for (;;) {
		/* Switch to parent bus */
		node_offset = parent->offset;
		parent = parent->parent;

		/* If root, we have finished */
		if (!parent || (parent->offset < 0)) {
			pr_debug("OF: reached root node\n");
			result = of_read_number(addr, na);
			break;
		}

		/* Get new parent bus and counts */
		pbus = &of_busses[0];
		pbus->count_cells(data, parent->offset, &pna, &pns);
		if (!of_check_counts(pna, pns)) {
			pr_err("%s: Bad cell count for %s\n",
					__func__, parent->name);
			break;
		}

		pr_debug("OF: parent bus is %s (na=%d, ns=%d) on %s\n",
				pbus->name, pna, pns, parent->name);

		/* Apply bus translation */
		if (of_translate_one(data, node_offset, bus, pbus,
					addr, na, ns, pna, "ranges"))
			break;

		/* Complete the move up one level */
		na = pna;
		ns = pns;
		bus = pbus;
	}
 bail:

	return result;
}

int of_translate_address_index(struct device_node *np,
		uint64_t *address, uint64_t *size, int index)
{
	const fdt32_t *reg;
	int na, ns, node, len = 0;
	void *data;

	if (!np || !np->data || (np->offset <=0))
		return -EINVAL;

	data = np->data;
	node = np->offset;
	na = fdt_n_addr_cells(np->data, np->offset);
	if (na < 1) {
		pr_debug("bad #address-cells %s\n", np->name);
		return -EINVAL;
	}

	ns = fdt_n_size_cells(np->data, np->offset);
	if (ns < 0) {
		pr_debug("bad #size-cells %s\n", np->name);
		return -EINVAL;
	}

	reg = fdt_getprop(data, node, "reg", &len);
	if (!reg || (len <= (index * 4) *(na + ns))) {
		pr_debug("index out of range\n");
		return -ENOENT;
	}

	reg += index * (na + ns);

	if (ns) {
		*address = __of_translate_address(np, reg, "ranges");
		if (*address == OF_BAD_ADDR)
			return -EINVAL;

		if (ns == 2)
			*size = fdt32_to_cpu64(reg[na], reg[na + 1]);
		else
			*size = fdt32_to_cpu(reg[na]);
	} else {
		*address = of_read_number(reg, na);
		*size = 0;
	}

	return 0;
}

int of_translate_address(struct device_node *node,
		uint64_t *address, uint64_t *size)
{
	return of_translate_address_index(node, address, size, 0);
}

int get_device_irq_index(struct device_node *node, uint32_t *irq,
		unsigned long *flags, int index)
{
	int irq_cells, len, i;
	of32_t *value;
	uint32_t irqv[4];

	if (!node)
		return -EINVAL;

	value = (of32_t *)of_getprop(node, "interrupts", &len);
	if (!value || (len < sizeof(of32_t)))
		return -ENOENT;

	irq_cells = of_n_interrupt_cells(node);
	if (irq_cells == 0) {
		pr_err("bad irqcells - %s\n", node->name);
		return -ENOENT;
	}

	pr_debug("interrupt-cells %d\n", irq_cells);

	len = len / sizeof(of32_t);
	if (index >= len)
		return -ENOENT;

	value += (index * irq_cells);
	for (i = 0; i < irq_cells; i++)
		irqv[i] = of32_to_cpu(*value++);

	return irq_xlate(node, irqv, irq_cells, irq, flags);
}

void *of_device_node_match(struct device_node *node, void *s, void *e)
{
	int i, count;
	struct module_id *module;

	if (e <= s)
		return NULL;

	count = (e - s) / sizeof(struct module_id);
	if (count == 0)
		return NULL;

	for (i = 0; i < count; i++) {
		module = (struct module_id *)s;
		if (of_device_match(node, (char **)module->comp))
			return module->data;

		s += sizeof(struct module_id);
	}

	return NULL;
}

void of_release_all_node(struct device_node *node)
{
	struct device_node *tmp = node;
	struct device_node *tmp2;

	if (!device_node_is_root(node))
		return;

	do {
		tmp2 = tmp->next;
		free(tmp);
		tmp = tmp2;
	} while (tmp);
}

struct device_node *of_parse_device_tree(void *data)
{
	struct device_node *root = NULL;

	if (!data)
		return NULL;

	if (fdt_check_header(data)) {
		pr_err("invaild dtb header for dt parsing\n");
		return NULL;
	}

	root = alloc_device_node();
	if (!root)
		return NULL;

	root->data = data;
	root->name = "root node";
	root->offset = 0;
	root->parent = NULL;
	root->sibling = NULL;
	root->next = NULL;
	root->class = DT_CLASS_OTHER;
	root->compatible = fdt_getprop(data, 0, "compatible", NULL);
	root->flags = DEVICE_NODE_F_OF | DEVICE_NODE_F_ROOT;

	/*
	 * now parse all the node and convert them to the
	 * device node struct for the hypervisor and vm0 use
	 */
	__of_parse_device_node(root, root);

	return root;
}
