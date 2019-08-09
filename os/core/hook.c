/*
 * Copyright (C) 2019 Min Le (lemin9538@gmail.com)
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
#include <minos/mm.h>

static struct list_head hook_lists[MINOS_HOOK_TYPE_UNKNOWN];

static int hooks_init(void)
{
	int i;

	for (i = 0; i < MINOS_HOOK_TYPE_UNKNOWN; i++)
		init_list(&hook_lists[i]);

	return 0;
}
early_initcall(hooks_init);

int register_hook(hook_func_t fn, enum hook_type type)
{
	struct hook *hook;

	if ((fn == NULL) || (type >= MINOS_HOOK_TYPE_UNKNOWN)) {
		pr_err("Hook info is invaild\n");
		return -EINVAL;
	}

	hook = malloc(sizeof(*hook));
	if (!hook)
		return -ENOMEM;

	memset(hook, 0, sizeof(*hook));
	hook->fn = fn;

	list_add_tail(&hook_lists[type], &hook->list);

	return 0;
}

int do_hooks(void *item, void *context, enum hook_type type)
{
	int err = 0;
	struct hook *hook;

	list_for_each_entry(hook, &hook_lists[type], list)
		err += hook->fn(item, context);

	return err;
}
