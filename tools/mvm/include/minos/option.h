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

#define OPTION_TAG_GLOBAL	"option_global"
#define OPTION_TAG_VM		"option_vm"
#define OPTION_TAG_OS		"option_os"
#define OPTION_TAG_VDEV		"option_vdev"

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

#define DEFINE_OPTION_PARSER(n, arg_name, tag_name, f, hdl)	\
	static struct mvm_option_parser __option_##n __used = {	\
		.name = arg_name,				\
		.handler = hdl,					\
		.force	= f,					\
	};							\
	static struct mvm_option_parser *__option_##n##_ptr	\
	__used __section(tag_name) = &__option_##n

#define DEFINE_OPTION_GLOBAL(n, arg_name, f, handler)	\
	DEFINE_OPTION_PARSER(n, arg_name, OPTION_TAG_GLOBAL, f, handler)

#define DEFINE_OPTION_VM(n, arg_name, f, handler)	\
	DEFINE_OPTION_PARSER(n, arg_name, OPTION_TAG_VM, f, handler)

#define DEFINE_OPTION_OS(n, arg_name, f, handler)	\
	DEFINE_OPTION_PARSER(n, arg_name, OPTION_TAG_OS, f, handler)

#define DEFINE_OPTION_VDEV(n, arg_name, f, handler)	\
	DEFINE_OPTION_PARSER(n, arg_name, OPTION_TAG_VDEV, f, handler)

extern unsigned char __start_option_vm;
extern unsigned char __stop_option_vm;
extern unsigned char __start_option_os;
extern unsigned char __stop_option_os;
extern unsigned char __start_option_vdev;
extern unsigned char __stop_option_vdev;

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
