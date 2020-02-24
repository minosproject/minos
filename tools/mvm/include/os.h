#ifndef __OS_H__
#define __OS_H__

#define DEFINE_OS(os) \
	static void *os_##os __section("mvm_os") __used = &os;

extern void *__start_mvm_os;
extern void *__stop_mvm_os;

struct vm;

struct vm_os {
	char *name;
	int (*early_init)(struct vm *vm);
	int (*load_image)(struct vm *vm);
	int (*setup_vm_env)(struct vm *vm, char *cmdline);
};

#define OS_TYPE_LINUX		(1 << 0)

#endif
