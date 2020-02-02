#ifndef __OS_H__
#define __OS_H__

#include <minos/mvm.h>

#define DEFINE_OS(os) \
	static void *os_##os __section("mvm_os") __used = &os;

extern void *__start_mvm_os;
extern void *__stop_mvm_os;

struct vm;

enum {
	OS_TYPE_OTHER = 0,
	OS_TYPE_LINUX,
	OS_TYPE_XNU,
};

struct vm_os {
	char *name;
	int type;
	int (*early_init)(struct vm *vm);
	int (*load_image)(struct vm *vm);
	int (*setup_vm_env)(struct vm *vm, char *cmdline);
	int (*create_resource)(struct vm *vm);
	void (*vm_exit)(struct vm *vm);
};

struct vm_os *get_vm_os(char *os_type);
int os_load_images(struct vm *vm);
int os_early_init(struct vm *vm);
int os_create_resource(struct vm *vm);
int os_setup_vm(struct vm *vm);

#endif
