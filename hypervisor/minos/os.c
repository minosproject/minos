/*
 * Copyright (C) 2018 Min Le (lemin9538@gmail.com)
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
#include <minos/sched.h>
#include <minos/virt.h>
#include <minos/os.h>

LIST_HEAD(os_list);
struct os *default_os;

static void default_vcpu_init(struct vcpu *vcpu)
{
	vcpu_online(vcpu);
}

struct os_ops default_os_ops = {
	.vcpu_init = default_vcpu_init,
};

int register_os(struct os *os)
{
	if ((!os) || (!os->ops))
		return -EINVAL;

	/* do not need lock now */
	list_add_tail(&os_list, &os->list);

	return 0;
}

struct os *alloc_os(char *name)
{
	struct os *os;

	os = (struct os *)zalloc(sizeof(struct os));
	if (!os)
		return NULL;

	init_list(&os->list);
	strncpy(os->name, name, MIN(strlen(name),
			MINOS_OS_NAME_SIZE - 1));

	return os;
}

struct os *get_vm_os(char *type)
{
	struct os *os;

	if (!type)
		goto out;

	list_for_each_entry(os, &os_list, list) {
		if (!strcmp(os->name, type))
			return os;
	}

out:
	return default_os;
}

static int os_init(void)
{
	default_os = alloc_os("default-os");
	if (!default_os)
		return -ENOMEM;

	default_os->ops = &default_os_ops;

	return register_os(default_os);
}

subsys_initcall(os_init);
