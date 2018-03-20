#include <mvisor/mvisor.h>
#include <mvisor/module.h>
#include <mvisor/init.h>
#include <mvisor/mm.h>
#include <mvisor/vcpu.h>
#include <mvisor/spinlock.h>

extern unsigned char __vmm_module_start;
extern unsigned char __vmm_module_end;

static struct list_head module_list;
static int module_class_nr;
static struct spinlock module_lock;

typedef int (*module_init_fn)(struct vmm_module *);

static char *vmm_module_names[] = {
	VMM_MODULE_NAME_IRQCHIP,
	VMM_MODULE_NAME_MMU,
	VMM_MODULE_NAME_SYSTEM,
	NULL,
};

static int get_module_id(char *type)
{
	int i;
	char *name;

	for (i = 0; ; i++) {
		name = vmm_module_names[i];
		if (name == NULL)
			break;

		if (strcmp(name, type) == 0)
			return i;
	}

	return INVAILD_MODULE_ID;
}

static void vmm_create_module(struct module_id *id)
{
	struct vmm_module *module;
	module_init_fn fn;

	module = (struct vmm_module *)
		vmm_malloc(sizeof(struct vmm_module));
	if (!module)
		panic("No more memory for vmm_module\n");

	memset((char *)module, 0, sizeof(struct vmm_module));
	strncpy(module->name, id->name, 31);
	strncpy(module->type, id->type, 31);
	init_list(&module->list);
	module->id = get_module_id(module->type);

	/* call init routine */
	if (id->fn) {
		fn = (module_init_fn)id->fn;
		fn(module);
	}

	list_add(&module_list, &module->list);
}

void *get_vcpu_module_data(vcpu_t *vcpu, char *name)
{
	uint32_t id;

	id = get_module_id(name);
	if (id >= sizeof(vmm_module_names) / sizeof(vmm_module_names[0]))
		return 0;

	return vcpu->module_context[id];
}

int vcpu_modules_init(vcpu_t *vcpu)
{
	struct list_head *list;
	struct vmm_module *module;
	void *data;
	int size;

	/*
	 * firset allocate memory to store each module
	 * context's context data
	 */
	size = module_class_nr * sizeof(void *);
	vcpu->module_context = (void **)vmm_malloc(size);
	if (!vcpu->module_context)
		panic("No more memory for vcpu module cotnext\n");

	memset((char *)vcpu->module_context, 0, size);

	list_for_each(&module_list, list) {
		module = list_entry(list, struct vmm_module, list);
		if (module->context_size) {
			data = (void *)vmm_malloc(module->context_size);
			memset((char *)data, 0, size);
			vcpu->module_context[module->id] = data;

			if (module->state_init)
				module->state_init(vcpu, data);
		}
	}

	return 0;
}

void *vmm_get_module_pdata(char *name, char *type)
{
	struct list_head *list;
	void *pdata = NULL;
	struct vmm_module *module;

	spin_lock(&module_lock);

	list_for_each(&module_list, list) {
		module = list_entry(list, struct vmm_module, list);
		if ((strcmp(module->type, type) == 0) &&
			(strcmp(module->name, name) == 0)) {
			pdata = module->pdata;
			break;
		}
	}

	spin_unlock(&module_lock);

	return pdata;
}

int vmm_modules_init(void)
{
	int32_t i;
	uint64_t base, end;
	uint32_t size;
	struct module_id *mid;

	init_list(&module_list);
	spin_lock_init(&module_lock);
	module_class_nr = 0;
	base = (uint64_t)&__vmm_module_start;
	end = (uint64_t)&__vmm_module_end;
	size = (&__vmm_module_end - &__vmm_module_start) /
			sizeof(struct module_id);

	for (i = 0; ; i++) {
		if (vmm_module_names[i] == NULL)
			break;
		module_class_nr++;
	}

	for (i = 0; i < size; i++) {
		mid = (struct module_id *)base;

		if (get_module_id(mid->type) == INVAILD_MODULE_ID) {
			pr_error("Find invaild module %s\n", mid->type);
			continue;
		}

		vmm_create_module(mid);
		base += sizeof(struct module_id);
	}

	return 0;
}

arch_initcall(vmm_modules_init);
