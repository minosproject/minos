#ifndef __MVM_OPTION_H__
#define __MVM_OPTION_H__

#include <minos/mvm.h>

enum {
	OPTION_GRP_GLOBAL = 0,
	OPTION_GRP_VM,
	OPTION_GRP_OS,
	OPTION_GRP_VDEV,
	OPTION_GRP_UNKNOWN,
};

#define OPTION_TAG_GLOBAL	global
#define OPTION_TAG_VM		vm
#define OPTION_TAG_OS		os
#define OPTION_TAG_VDEV		vdev

struct mvm_option_parser {
	int force;
	char *name;
	int (*handler)(char *arg, char *sub_arg, void *data);
};

struct mvm_option {
	char *name;
	char *args;
	char *sub_args;
	struct list_head list;
};

#define OPTION_NAME(tag, n)	__option_##tag##_##n
#define VM_OPTION(n)		OPTION_NAME(vm, n)
#define OS_OPTION(n)		OPTION_NAME(os, n)
#define VDEV_OPTION(n)		OPTION_NAME(vdev, n)

#define DEFINE_OPTION_PARSER(n, arg_name, tag_name, f, hdl)	\
	struct mvm_option_parser OPTION_NAME(tag_name, n) __used = {	\
		.name = arg_name,				\
		.handler = hdl,					\
		.force	= f,					\
	};

#define DEFINE_OPTION_GLOBAL(n, arg_name, f, handler)	\
	DEFINE_OPTION_PARSER(n, arg_name, OPTION_TAG_GLOBAL, f, handler)

#define DEFINE_OPTION_VM(n, arg_name, f, handler)	\
	DEFINE_OPTION_PARSER(n, arg_name, OPTION_TAG_VM, f, handler)

#define DEFINE_OPTION_OS(n, arg_name, f, handler)	\
	DEFINE_OPTION_PARSER(n, arg_name, OPTION_TAG_OS, f, handler)

#define DEFINE_OPTION_VDEV(n, arg_name, f, handler)	\
	DEFINE_OPTION_PARSER(n, arg_name, OPTION_TAG_VDEV, f, handler)

#define DECLARE_VM_OPTION(n)	\
	extern struct mvm_option_parser VM_OPTION(n)
#define DECLARE_OS_OPTION(n)	\
	extern struct mvm_option_parser OS_OPTION(n)
#define DECLARE_VDEV_OPTION(n)	\
	extern struct mvm_option_parser VDEV_OPTION(n)

#define VM_OP(n)	&VM_OPTION(n)
#define OS_OP(n)	&OS_OPTION(n)
#define VDEV_OP(n)	&VDEV_OPTION(n)

int mvm_option_init(int argc, char **argv);
int mvm_parse_option_group(int group, void *data);
void mvm_free_options(void);

int mvm_parse_option_hex64(char *name, uint64_t *value);
int mvm_parse_option_hex32(char *name, uint32_t *value);
int mvm_parse_option_int(char *name, int *value);
int mvm_parse_option_uint32(char *name, uint32_t *value);
int mvm_parse_option_string(char *name, char **value);
int mvm_parse_option_bool(char *name, int *value);

/*
 * get value from option follow below format
 * xxx=xxx,xxx=xxx
 */
int get_option_hex64(char *args, char *item, uint64_t *value);
int get_option_hex32(char *args, char *item, uint64_t *value);
int get_option_int(char *args, char *item, uint64_t *value);
int get_option_uint32(char *args, char *item, uint64_t *value);
int get_option_string(char *args, char *item, char *str, int len);

#endif
