/*
 * BSD 3-Clause License
 *
 * Copyright (C) 2018 Min Le (lemin9538@gmail.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>

#include <minos/vm.h>
#include <minos/option.h>

static struct list_head option_head;

DECLARE_VM_OPTION(mem_size);
DECLARE_VM_OPTION(name);
DECLARE_VM_OPTION(os_type);
DECLARE_VM_OPTION(vcpus);
DECLARE_VM_OPTION(bootimage);
DECLARE_VM_OPTION(kfd);
DECLARE_VM_OPTION(dfd);
DECLARE_VM_OPTION(rfd);
DECLARE_VM_OPTION(rf);
DECLARE_VM_OPTION(entry);
DECLARE_VM_OPTION(setup_data);
DECLARE_VM_OPTION(setup_mem_base);
DECLARE_VM_OPTION(type);
DECLARE_VM_OPTION(cmdline);
DECLARE_VM_OPTION(gic);

DECLARE_VDEV_OPTION(device);

static struct mvm_option_parser *vm_parser_table[] = {
	VM_OP(mem_size),
	VM_OP(name),
	VM_OP(os_type),
	VM_OP(vcpus),
	VM_OP(bootimage),
	VM_OP(kfd),
	VM_OP(dfd),
	VM_OP(rfd),
	VM_OP(rf),
	VM_OP(entry),
	VM_OP(setup_data),
	VM_OP(setup_mem_base),
	VM_OP(type),
	VM_OP(cmdline),
	VM_OP(gic),
};

static struct mvm_option_parser *os_parser_table[] = {

};

static struct mvm_option_parser *vdev_parser_table[] = {
	VDEV_OP(device),
};

static void mvm_get_one_option(char *arg)
{
	char *ret = NULL;
	char *tmp = arg;
	struct mvm_option *option;

	option = (struct mvm_option *)calloc(1, sizeof(struct mvm_option));
	if (NULL == option) {
		pr_err("no memory for options\n");
		return;
	}

	/*
	 * first find @ then find first =, if it has @ means this is
	 * a mutil leavel argment otherwise it is a simple argument,
	 * strsep will return the head of the string. cmdline can not
	 * use;, since the string will be split when meet ;.
	 */
	ret = strsep(&tmp, "@");
	if (tmp != NULL) {
		option->name = ret;
		option->args = tmp;

		ret = strsep(&tmp, ",");
		if (tmp != NULL)
			option->sub_args = tmp;
		else
			pr_warn("argment format wrong: %s\n", arg);
	} else {
		option->name = arg;
		option->sub_args = NULL;

		tmp = arg;
		ret = strsep(&tmp, "=");
		option->args = tmp;
	}

	list_add_tail(&option_head, &option->list);
}

static int inline get_option_parser(int group,
		struct mvm_option_parser ***start, int *cnt)
{
	switch (group) {
	case OPTION_GRP_VM:
		*start = vm_parser_table;
		*cnt = sizeof(vm_parser_table) / sizeof(vm_parser_table[0]);
		break;
	case OPTION_GRP_OS:
		*start = os_parser_table;
		*cnt = sizeof(os_parser_table) / sizeof(os_parser_table[0]);
		break;
	case OPTION_GRP_VDEV:
		*start = vdev_parser_table;
		*cnt = sizeof(vdev_parser_table) / sizeof(vdev_parser_table[0]);
		break;
	default:
		pr_err("unsupport option group %d\n", group);
		return -EINVAL;
	}

	return 0;
}

static int inline __parse_option(char *name, void *value,
		int (*parser)(char *args, void *value))
{
	int ret;
	struct mvm_option *option, *tmp;

	list_for_each_entry_safe(option, tmp, &option_head, list) {
		if (strcmp(name, option->name) != 0)
			continue;

		ret = parser(option->args, value);
		list_del(&option->list);
		free(option);

		return ret;
	}

	return -ENOENT;

}

static int __parse_hex32(char *args, void *value)
{
	if (args) {
		sscanf((const char *)args, "0x%x", (uint32_t *)value);
		return 0;
	}

	return -EINVAL;
}

static int __parse_hex64(char *args, void *value)
{
	if (args) {
		sscanf((const char *)args, "0x%"PRIx64, (uint64_t *)value);
		return 0;
	}

	return -EINVAL;
}

static int __parse_int(char *args, void *value)
{
	if (args) {
		sscanf((const char *)args, "0x%d", (int32_t *)value);
		return 0;
	}

	return -EINVAL;
}

static int __parse_u32(char *args, void *value)
{
	if (args) {
		sscanf((const char *)args, "0x%u", (uint32_t *)value);
		return 0;
	}

	return -EINVAL;
}

static int __parse_bool(char *args, void *value)
{
	*(int *)value = 1;

	return 0;
}

static int __parse_string(char *args, void *value)
{
	if (args) {
		*(char **)value = args;
		return 0;
	}

	return -EINVAL;
}

static int find_char_pos(char *args, char **pos1, char **pos2)
{
	*pos1 = NULL;
	*pos2 = NULL;

	if (*args == 0)
		return 0;

	while (*args != 0) {
		if (*args == '=') {
			*pos1 = args;
		} else if ((*args == ',') || (*args == 0)){
			if (*pos1) {
				*pos2 = args;
				return 1;
			} else {
				return 0;
			}
		}

		args++;
	}

	/*
	 * if reach the end of the string
	 */
	if (*pos1) {
		*pos2 = args;
		return 1;
	}

	return 0;
}

static int __get_option(char *args, char *item, void *val,
		int (*parse)(char *args, void *value))
{
	int ret;
	char backup;
	char *start = args;
	char *pos_1, *pos_2;
	int len_1, len_2;

	if (!args || !item)
		return -EINVAL;

	/*
	 * skip the unused , on the head of the string
	 */
	while (*start) {
		if (*start == ',')
			start++;
		else
			break;
	}

	if (*start == 0)
		return -EINVAL;

	/*
	 * find the position of = and , pos_1 is the position
	 * of = and pos_2 is the postion of , and null
	 */
	while (find_char_pos(start, &pos_1, &pos_2)) {
		len_1 = pos_1 - start;
		len_2 = pos_2 - pos_1;

		if ((len_1 == 0) || (len_2 == 0) || (len_1 != strlen(item)))
			goto repeat;

		if (strncmp(start, item, len_1) == 0) {
			backup = *pos_2;
			*pos_2 = 0;
			ret = parse(pos_1 + 1, val);
			*pos_2 = backup;

			return ret;
		}

repeat:
		start = pos_2;
		if (*start == ',')
			start++;
	}

	return -ENOENT;
}

int get_option_hex64(char *args, char *item, uint64_t *value)
{
	return __get_option(args, item, (void *)value, __parse_hex64);
}

int get_option_hex32(char *args, char *item, uint64_t *value)
{
	return __get_option(args, item, (void *)value, __parse_hex32);
}

int get_option_int(char *args, char *item, uint64_t *value)
{
	return __get_option(args, item, (void *)value, __parse_int);
}

int get_option_uint32(char *args, char *item, uint64_t *value)
{
	return __get_option(args, item, (void *)value, __parse_u32);
}

int get_option_string(char *args, char *item, char *str, int len)
{
	char *val;
	char *end;
	int ret;

	ret = __get_option(args, item, (void *)&val, __parse_string);
	if (ret)
		return ret;

	/*
	 * find the next , or 0
	 */
	end = val;

	while (1) {
		if ((*end == ',') || (*end == 0))
			break;

		end++;
	}

	if (end == val)
		return 0;

	ret = end - val;
	if (ret > len)
		return 0;

	strncpy(str, val, ret);

	return ret;
}

int mvm_parse_option_hex64(char *name, uint64_t *value)
{
	return __parse_option(name, value, __parse_hex64);
}

int mvm_parse_option_hex32(char *name, uint32_t *value)
{
	return __parse_option(name, value, __parse_hex32);
}

int mvm_parse_option_uint32(char *name, uint32_t *value)
{
	return __parse_option(name, value, __parse_u32);
}

int mvm_parse_option_int(char *name, int32_t *value)
{
	return __parse_option(name, value, __parse_int);
}

int mvm_parse_option_string(char *name, char **value)
{
	return __parse_option(name, value, __parse_string);
}

int mvm_parse_option_bool(char *name, int *value)
{
	*value = 0;

	return __parse_option(name, value, __parse_bool);
}

int mvm_parse_option_group(int group, void *data)
{
	int found = 0, ret;
	int i= 0, cnt;
	struct mvm_option *option, *tmp;
	struct mvm_option_parser *p;
	struct mvm_option_parser **p_start;

	if (get_option_parser(group, &p_start, &cnt) || (cnt == 0))
		return -EINVAL;

	/*
	 * search all the option which need to be parsed
	 * int the specific group, if the option is a force
	 * option need to check it's state, if fail then return
	 * error
	 */
	for (i = 0; i < cnt; i++) {
		p = p_start[i];
		list_for_each_entry_safe(option,
				tmp, &option_head, list) {
			if (strcmp(option->name, p->name) != 0)
				continue;

			ret = p->handler(option->args, option->sub_args, data);
			list_del(&option->list);
			free(option);

			if (ret && p->force) {
				pr_err("error option: %s\n", p->name);
				return -EINVAL;
			}

			found = 1;
			break;
		}

		if ((found == 0) && (p->force)) {
			pr_err("please pass option: %s=\n", p->name);
			return -ENOENT;
		}

		found = 0;
	}

	return 0;
}

void mvm_free_options(void)
{
	struct mvm_option *option, *tmp;

	pr_info("mvm free all options\n");

	list_for_each_entry_safe(option, tmp, &option_head, list) {
		list_del(&option->list);
		free(option);
	}
}

/*
 * ./mvm vcpu=4 memory=48 device@virtio_console;port=4
 */
int mvm_option_init(int argc, char **argv)
{
	int i;

	init_list(&option_head);

	if (argc < 2)
		return -EINVAL;

	for (i = 1; i < argc; i++) {
		if (argv[i] == NULL)
			break;

		mvm_get_one_option(argv[i]);
	}

	return 0;
}
