#ifndef __MINOS_BOOTARG_H__
#define __MINOS_BOOTARG_H__

int bootargs_init(const char *str, int len);

int __get_boot_option(char *name, void *value,
		int (*parse)(char *args, void *value));

int bootarg_parse_hex32(char *name, uint32_t *v);
int bootarg_parse_hex64(char *name, uint64_t *v);
int bootarg_parse_uint(char *name, uint32_t *v);
int bootarg_parse_bool(char *name, int *v);
int bootarg_parse_string(char *name, char **v);

#endif
