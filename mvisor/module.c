#include <mvisor/mvisor.h>
#include <mvisor/module.h>
#include <mvisor/init.h>
#include <mvisor/mm.h>
#include <mvisor/vcpu.h>
#include <mvisor/spinlock.h>

extern unsigned char __vmm_module_start;
extern unsigned char __vmm_module_end;

static struct list_head module_list;
static struct spinlock module_lock;
static int module_class_nr = 0;

typedef int (*module_init_fn)(struct vmm_module *);

int get_module_id(char *type)
{
	struct vmm_module *module;

	list_for_each_entry(module, &module_list, list) {
		if (strcmp(module->type, type) == 0)
			return module->id;
	}

	return INVAILD_MODULE_ID;
}

static struct vmm_module *vmm_create_module(struct module_id *id)
{
	struct vmm_module *module;
	module_init_fn fn;

	module = (struct vmm_module *)
		vmm_malloc(sizeof(struct vmm_module));
	if (!module) {
		pr_error("No more memory for vmm_module\n");
		return NULL;
	}

	memset((char *)module, 0, sizeof(struct vmm_module));
	strncpy(module->name, id->name, 31);
	strncpy(module->type, id->type, 31);
	init_list(&module->list);
	module->id = module_class_nr;
	module_class_nr++;

	/* call init routine */
	if (id->fn) {
		fn = (module_init_fn)id->fn;
		fn(module);
	}

	list_add(&module_list, &module->list);
	return module;
}

void *get_module_data_by_name(vcpu_t *vcpu, char *name)
{
	uint32_t id;

	id = get_module_id(name);
	if (id == INVAILD_MODULE_ID)
		return NULL;

	return vcpu->module_context[id];
}

void *get_module_data_by_id(vcpu_t *vcpu, int id)
{
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

void modules_create_vm(vm_t *vm)
{
	struct vmm_module *module;

	list_for_each_entry(module, &module_list, list) {
		if (module->create_vm)
			module->create_vm(vm);
	}
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

void restore_vcpu_module_state(vcpu_t *vcpu)
{
	struct vmm_module *module;
	void *context;

	list_for_each_entry(module, &module_list, list) {
		if (module->state_restore) {
			context = get_module_data_by_id(vcpu, module->id);
			module->state_restore(vcpu, context);
		}
	}
}

void save_vcpu_module_state(vcpu_t *vcpu)
{
	struct vmm_module *module;
	void *context;

	list_for_each_entry(module, &module_list, list) {
		if (module->state_save) {
			context = get_module_data_by_id(vcpu, module->id);
			module->state_save(vcpu, context);
		}
	}
}

int vmm_modules_init(void)
{
	int32_t i;
	uint64_t base, end;
	uint32_t size;
	struct module_id *mid;
	struct vmm_module *module;

	init_list(&module_list);
	spin_lock_init(&module_lock);

	base = (uint64_t)&__vmm_module_start;
	end = (uint64_t)&__vmm_module_end;
	size = (&__vmm_module_end - &__vmm_module_start) /
			sizeof(struct module_id);

	for (i = 0; i < size; i++) {
		mid = (struct module_id *)base;
		module = vmm_create_module(mid);
		if (!module)
			pr_error("Can not create module\n");

		base += sizeof(struct module_id);
	}

	return 0;
}

arch_initcall(vmm_modules_init);
