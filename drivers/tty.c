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
#include <minos/tty.h>
#include <minos/mm.h>

static LIST_HEAD(tty_list);
static DEFINE_SPIN_LOCK(tty_lock);

struct tty *alloc_tty(char *name, uint32_t id, unsigned long flags)
{
	struct tty *tty;

	list_for_each_entry(tty, &tty_list, list) {
		if ((tty->id == id) || (strcmp(name, tty->name)==0)) {
			pr_err("tty is areadly register 0x%x\n", id);
			return NULL;
		}
	}

	tty = zalloc(sizeof(struct tty));
	if (!tty)
		return NULL;

	tty->id = id;
	tty->flags = flags;
	strncpy(tty->name, name, TTY_NAME_SIZE - 1);

	return tty;
}
EXPORT_SYMBOL(alloc_tty);

int register_tty(struct tty *tty)
{
	unsigned long iflags;

	if (tty->ops == NULL)
		return -EINVAL;

	spin_lock_irqsave(&tty_lock, iflags);
	list_add_tail(&tty_list, &tty->list);
	spin_unlock_irqrestore(&tty_lock, iflags);

	return 0;
}
EXPORT_SYMBOL(register_tty);

int release_tty(struct tty *tty)
{
	unsigned long flags;

	if (!tty || (tty->list.next == NULL))
		return -EINVAL;

	spin_lock_irqsave(&tty_lock, flags);
	list_del(&tty->list);
	spin_unlock_irqrestore(&tty_lock, flags);

	free(tty);

	return 0;
}
EXPORT_SYMBOL(release_tty);

struct tty *open_tty(char *name)
{
	struct tty *tty, *__tty = NULL;

	list_for_each_entry(tty, &tty_list, list) {
		if (strcmp(name, tty->name) == 0) {
			__tty = tty;
			break;
		}
	}

	if (!__tty || __tty->open)
		return NULL;

	if (tty->ops->open)
		tty->ops->open(tty);
	tty->open = 1;

	return __tty;
}
EXPORT_SYMBOL(open_tty);

void close_tty(struct tty *tty)
{
	if (!tty || !tty->open)
		return;

	if (tty->ops->close)
		tty->ops->close(tty);
	tty->open = 0;
}
EXPORT_SYMBOL(close_tty);
