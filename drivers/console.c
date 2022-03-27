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
#include <minos/console.h>
#include <minos/of.h>
#include <minos/spinlock.h>
#include <minos/sched.h>

#define MEM_CONSOLE_SIZE	(2048)
#define CONSOLE_INBUF_SIZE	(2048)

static int widx;
static char mem_log_buf[MEM_CONSOLE_SIZE];

static char console_inbuf[CONSOLE_INBUF_SIZE];
static uint32_t inbuf_ridx, inbuf_widx;
static DEFINE_SPIN_LOCK(inbuf_lock);
static struct task *inbuf_task;

#define MEM_CONSOLE_IDX(idx)	(idx & (MEM_CONSOLE_SIZE - 1))
#define BUFIDX(idx)		(idx & (CONSOLE_INBUF_SIZE - 1))

static void mem_console_putc(char ch)
{
	mem_log_buf[MEM_CONSOLE_IDX(widx++)] = ch;
}

static char mem_console_getc(void)
{
	return 0;
}
DEFINE_CONSOLE(mem_console, "mem-console", NULL,
		mem_console_putc, mem_console_getc);

static struct console *console = &__console_mem_console;

struct console *get_console(char *name)
{
	extern unsigned long __console_start;
	extern unsigned long __console_end;
	struct console *console;

	if (!name)
		return NULL;

	section_for_each_item(__console_start, __console_end, console) {
		if (strcmp(console->name, name) == 0)
			return console;
	}

	return NULL;
}

void console_init(char *name)
{
	int index;

	if (name)
		console = get_console(name);

	if (console->init)
		console->init(NULL);

	/* flush the string in the memory log buf */
	for (index = 0; index < MEM_CONSOLE_IDX(widx); index++)
		console->putc(mem_log_buf[index]);
}

void console_putc(char ch)
{
	console->putc(ch);
}

char console_getc(void)
{
	return console->getc();
}

void console_char_recv(unsigned char ch)
{
	uint32_t widx;

	widx = inbuf_widx;
	console_inbuf[BUFIDX(widx++)] = ch;
	inbuf_widx = widx;
	mb();

	raw_spin_lock(&inbuf_lock);
	if (inbuf_task)
		wake_up(inbuf_task);
	raw_spin_unlock(&inbuf_lock);
}

void console_puts(char *buf, int len)
{
	puts(buf, len);
}

int console_gets(char *buf, int max)
{
	uint32_t ridx, widx;
	long i, copy;
	unsigned long flags;

repeat:
	ridx = inbuf_ridx;
	widx = inbuf_widx;
	mb();

	ASSERT((widx - ridx) <= CONSOLE_INBUF_SIZE);
	copy = widx - ridx > max ? max : widx - ridx;
	if (copy == 0) {
		spin_lock_irqsave(&inbuf_lock, flags);
		if (inbuf_widx - inbuf_ridx == 0) {
			__event_task_wait(0, TASK_EVENT_IRQ, -1);
			inbuf_task = current;
		}
		spin_unlock_irqrestore(&inbuf_lock, flags);

		if (inbuf_task) {
			wait_event();
			inbuf_task = NULL;
			if (copy < 0)
				return copy;
		}

		goto repeat;
	}

	for (i = 0; i < copy; i++)
		buf[i] = console_inbuf[BUFIDX(ridx++)];

	inbuf_ridx = ridx;
	mb();

	return copy;
}
