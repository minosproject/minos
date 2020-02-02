/*
 * BSD 3-Clause License
 *
 * Copyright (C) 2018 Min Le (lemin9538@gmail.com)
 * All rights reserved.
 */

#include <minos/vm.h>
#include <minos/os.h>
#include <minos/option.h>

#ifdef __CLANG__

extern struct vm_os os_linux;
extern struct vm_os os_xnu;
extern struct vm_os os_other;

static struct vm_os *vm_oses[] = {
	&os_linux,
	&os_xnu,
	&os_other,
	NULL,
};

struct vm_os *get_vm_os(char *os_type)
{
	int i;
	struct vm_os *os;

	for (i = 0; ; i++) {
		os = vm_oses[i];
		if (os == NULL)
			break;

		if (strcmp(os_type, os->name) == 0)
			return os;
	}

	return &os_other;
}
#else
struct vm_os *get_vm_os(char *os_type)
{
	struct vm_os **os_start = (struct vm_os **)&__start_mvm_os;
	struct vm_os **os_end = (struct vm_os **)&__stop_mvm_os;
	struct vm_os *default_os = NULL;
	struct vm_os *os;

	for (; os_start < os_end; os_start++) {
		os = *os_start;
		if (strcmp(os_type, os->name) == 0)
			return os;

		if (strcmp("default", os->name) == 0)
			default_os = os;
	}

	return default_os;
}
#endif

int os_load_images(struct vm *vm)
{
	if (vm->os->load_image)
		return vm->os->load_image(vm);

	return 0;
}

int os_early_init(struct vm *vm)
{
	if (vm->os->early_init)
		return vm->os->early_init(vm);

	return 0;
}

int os_create_resource(struct vm *vm)
{
	if (vm->os->create_resource)
		return vm->os->create_resource(vm);

	return 0;
}

int os_setup_vm(struct vm *vm)
{
	if (vm->os->setup_vm_env)
		return vm->os->setup_vm_env(vm, vm->cmdline);

	return 0;
}

/*
 * do nothing to fix compile issue for option_os section
 */
DEFINE_OPTION_OS(setup_os_null, "os-res", 0, NULL);
