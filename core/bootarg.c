/*
 * Copyright (C) 2020 Min Le (lemin9538@gmail.com)
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
#include <minos/bootarg.h>
#include <minos/bootmem.h>

struct boot_option {
	char *name;
	char *args;
	char *sub_args;
	struct boot_option *next;
};

static struct boot_option *boot_options;

int __get_boot_option(char *name, void *value,
		int (*parse)(char *args, void *value))
{
	struct boot_option *bo;

	for (bo = boot_options; bo != NULL; bo = bo->next) {
		if (strcmp(name, bo->name) != 0)
			continue;

		return parse(bo->args, value);
	}

	return -ENOENT;
}

static int __parse_hex32(char *args, void *value)
{
	if (!args)
		return -EINVAL;

	*(uint32_t *)value = strtoul(args, NULL, 16);
	return 0;
}

static int __parse_hex64(char *args, void *value)
{
	if (!args)
		return -EINVAL;

	*(uint64_t *)value = strtoul(args, NULL, 16);
	return 0;
}

static int __parse_uint(char *args, void *value)
{
	if (!args)
		return -EINVAL;

	*(uint32_t *)value = strtoul(args, NULL, 10);
	return 0;
}

static int __parse_bool(char *args, void *value)
{
	*(int*)value = 1;
	return 0;
}

static int __parse_string(char *args, void *value)
{
	if (!args)
		return -EINVAL;

	*(char **)value = args;

	return 0;
}

int bootarg_parse_hex32(char *name, uint32_t *v)
{
	return __get_boot_option(name, v, __parse_hex32);
}

int bootarg_parse_hex64(char *name, uint32_t *v)
{
	return __get_boot_option(name, v, __parse_hex64);
}

int bootarg_parse_uint(char *name, uint32_t *v)
{
	return __get_boot_option(name, v, __parse_uint);
}

int bootarg_parse_bool(char *name, int *v)
{
	*v = 0;
	return __get_boot_option(name, v, __parse_bool);
}

int bootarg_parse_string(char *name, char **v)
{
	return __get_boot_option(name, v, __parse_string);
}

static void bootarg_init_one(char *str)
{
	struct boot_option *bo;

	/*
	 * this function will called before mm_init so
	 * call alloc_boot_mem, the bootarg format is like
	 * xxx=xxx xxx=xxx xxx=xxx
	 */
	if ((*str == 0) || (*str == ' '))
		return;

	bo = alloc_boot_mem(sizeof(struct boot_option));
	if (!bo)
		panic("no more boot memory for boot argument\n");

	memset(bo, 0, sizeof(struct boot_option));
	bo->name = strsep(&str, "=");
	bo->args = str;

	bo->next = boot_options;
	boot_options = bo;
}

int __init_text bootargs_init(const char *str)
{
	char *tmp = (char *)str;
	char *bootarg;

	pr_notice("bootargs: %s\n", str);

	while ((bootarg = strsep(&tmp, " ")) != NULL)
		bootarg_init_one(bootarg);

	return 0;
}
