/*
 * esh - embedded shell
 * Copyright (C) 2017 Chris Pavlina
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include "esh.h"
#define ESH_INTERNAL_INCLUDE
#include "esh_argparser.h"
#include "esh_internal.h"
#include <minos/string.h>

enum esh_flags {
    IN_ESCAPE = 0x01,
    IN_BRACKET_ESCAPE = 0x02,
    IN_NUMERIC_ESCAPE = 0x04,
};

static struct esh * allocate_esh(void);
static void free_last_allocated(struct esh * esh);
static void do_print_callback(struct esh * esh, char c);
static void do_command(struct esh * esh, int argc, char ** argv);
static void do_overflow_callback(struct esh * esh, char const * buffer);
static bool command_is_nop(struct esh * esh);
static void execute_command(struct esh * esh);
static void handle_char(struct esh * esh, char c);
static void handle_esc(struct esh * esh, char esc);
static void handle_ctrl(struct esh * esh, char c);
static void ins_del(struct esh * esh, char c);
static void term_cursor_move(struct esh * esh, int n);
static void cursor_move(struct esh * esh, int n);
static void word_move(struct esh * esh, int dir);

void esh_default_overflow(struct esh * esh, char const * buffer, void * arg);

#ifdef ESH_STATIC_CALLBACKS
extern void ESH_PRINT_CALLBACK(struct esh * esh, char c, void * arg);
extern void ESH_COMMAND_CALLBACK(
    struct esh * esh, int argc, char ** argv, void * arg);
__attribute__((weak))
void ESH_OVERFLOW_CALLBACK(struct esh * esh, char const * buffer, void * arg)
{
    (void) arg;
    esh_default_overflow(esh, buffer, arg);
}
#else
void esh_register_command(struct esh * esh, esh_cb_command callback)
{
    (void) esh;
    ESH_INSTANCE->cb_command = callback;
}


void esh_register_print(struct esh * esh, esh_cb_print callback)
{
    (void) esh;
    ESH_INSTANCE->print = callback;
}


void esh_register_overflow(struct esh * esh, esh_cb_overflow overflow)
{
    (void) esh;
    ESH_INSTANCE->overflow = (overflow ? overflow : &esh_default_overflow);
}
#endif

// API WARNING: This function is separately declared in lib.rs
void esh_set_command_arg(struct esh * esh, void * arg)
{
    (void) esh;
    ESH_INSTANCE->cb_command_arg = arg;
}

// API WARNING: This function is separately declared in lib.rs
void esh_set_print_arg(struct esh * esh, void * arg)
{
    (void) esh;
    ESH_INSTANCE->cb_print_arg = arg;
}

// API WARNING: This function is separately declared in lib.rs
void esh_set_overflow_arg(struct esh * esh, void * arg)
{
    (void) esh;
    ESH_INSTANCE->cb_overflow_arg = arg;
}

static void do_print_callback(struct esh * esh, char c)
{
    (void) esh;
#ifdef ESH_STATIC_CALLBACKS
    ESH_PRINT_CALLBACK(ESH_INSTANCE, c, ESH_INSTANCE->cb_print_arg);
#else
    ESH_INSTANCE->print(ESH_INSTANCE, c, ESH_INSTANCE->cb_print_arg);
#endif
}


static void do_command(struct esh * esh, int argc, char ** argv)
{
    (void) esh;
#ifdef ESH_STATIC_CALLBACKS
    ESH_COMMAND_CALLBACK(ESH_INSTANCE, argc, argv, ESH_INSTANCE->cb_command_arg);
#else
    ESH_INSTANCE->cb_command(ESH_INSTANCE, argc, argv, ESH_INSTANCE->cb_command_arg);
#endif
}


static void do_overflow_callback(struct esh * esh, char const * buffer)
{
    (void) esh;
#ifdef ESH_STATIC_CALLBACKS
    ESH_OVERFLOW_CALLBACK(ESH_INSTANCE, buffer, ESH_INSTANCE->cb_overflow_arg);
#else
    ESH_INSTANCE->overflow(ESH_INSTANCE, buffer, ESH_INSTANCE->cb_overflow_arg);
#endif
}


/**
 * For the static allocator, this is global so free_last_allocated() can
 * decrement it.
 */
#if ESH_ALLOC == STATIC
static bool g_allocated = false;
struct esh g_esh_struct;
#endif

/**
 * Allocate a new struct esh, or return a new statically allocated one from the pool.
 * This does not perform initialization.
 */
static struct esh * allocate_esh(void)
{
#if ESH_ALLOC == STATIC
    if (g_allocated) {
        return NULL;
    } else {
        g_allocated = true;
        return &g_esh_struct;
    }
#elif ESH_ALLOC == MALLOC
    return malloc(sizeof(struct esh));
#else
#   error "ESH_ALLOC must be STATIC or MALLOC"
#endif
}


/**
 * Free the last struct esh that was allocated, in case an initialization error
 * occurs after allocation.
 */
static void free_last_allocated(struct esh *esh)
{
#if ESH_ALLOC == STATIC
    (void) esh;
    g_allocated = false;
#elif ESH_ALLOC == MALLOC
    free(esh);
#endif
}


// API WARNING: This function is separately declared in lib.rs
struct esh * esh_init(void)
{
    struct esh * esh = allocate_esh();

    memset(esh, 0, sizeof(*esh));
#ifndef ESH_STATIC_CALLBACKS
    esh->overflow = &esh_default_overflow;
#endif

    if (esh_hist_init(ESH_INSTANCE)) {
        free_last_allocated(ESH_INSTANCE);
        return NULL;
    } else {
        return esh;
    }
}


// API WARNING: This function is separately declared in lib.rs
void esh_rx(struct esh * esh, char c)
{
    (void) esh;
    if (ESH_INSTANCE->flags & (IN_BRACKET_ESCAPE | IN_NUMERIC_ESCAPE)) {
        handle_esc(ESH_INSTANCE, c);
    } else if (ESH_INSTANCE->flags & IN_ESCAPE) {
        if (c == '[' || c == 'O') {
            ESH_INSTANCE->flags |= IN_BRACKET_ESCAPE;
        } else {
            ESH_INSTANCE->flags &= ~(IN_ESCAPE | IN_BRACKET_ESCAPE);
        }
    } else {
        // Verify the c is valid non-extended ASCII (and thus also valid
        // UTF-8, for Rust), regardless of whether this platform's isprint()
        // accepts things above 0x7f.
        if (c >= 0x20 && (unsigned char) c < 0x7f) {
            handle_char(ESH_INSTANCE, c);
        } else {
            handle_ctrl(ESH_INSTANCE, c);
        }
    }
}


/**
 * Process a normal text character. If there is room in the buffer, it is
 * inserted directly. Otherwise, the buffer is set into the overflow state.
 */
static void handle_char(struct esh * esh, char c)
{
    (void) esh;
    esh_hist_substitute(ESH_INSTANCE);

    if (ESH_INSTANCE->cnt < ESH_BUFFER_LEN) {
        ins_del(ESH_INSTANCE, c);
    } else {
        // If we let esh->cnt keep counting past the buffer limit, it could
        // eventually wrap around. Let it sit right past the end, and make sure
        // there is a NUL terminator in the buffer (we promise the overflow
        // handler that).
        ESH_INSTANCE->cnt = ESH_BUFFER_LEN + 1;

        // Note that the true buffer length is actually one greater than
        // ESH_BUFFER_LEN (which is the number of characters NOT including the
        // terminator that it can hold).
        ESH_INSTANCE->buffer[ESH_BUFFER_LEN] = 0;
    }
}


/**
 * Process a single-character control byte.
 */
static void handle_ctrl(struct esh * esh, char c)
{
    (void) esh;
    switch (c) {
        case 27: // escape
            ESH_INSTANCE->flags |= IN_ESCAPE;
            break;
        case 3:  // ^C
            esh_puts_flash(ESH_INSTANCE, FSTR("^C\n"));
            esh_print_prompt(ESH_INSTANCE);
            ESH_INSTANCE->cnt = ESH_INSTANCE->ins = 0;
            break;
        case '\n':
            execute_command(ESH_INSTANCE);
            break;
        case 8:     // backspace
        case 127:   // delete
            esh_hist_substitute(ESH_INSTANCE);
            if (ESH_INSTANCE->cnt > 0 && ESH_INSTANCE->cnt <= ESH_BUFFER_LEN) {
                ins_del(ESH_INSTANCE, 0);
            }
            break;
        default:
            // nop
            ;
    }
}


/**
 * Process the last character in an escape sequence.
 */
static void handle_esc(struct esh * esh, char esc)
{
    (void) esh;
    int cdelta;

    if (esc >= '0' && esc <= '9') {
        ESH_INSTANCE->flags
            |= IN_ESCAPE | IN_BRACKET_ESCAPE | IN_NUMERIC_ESCAPE;
        return;
    }

    if (ESH_INSTANCE->flags & IN_NUMERIC_ESCAPE) {
        // Numeric escapes can contain numbers and semicolons; they terminate
        // at letters and ~
        if (esc == '~' || isalpha(esc)) {
            ESH_INSTANCE->flags
                &= ~(IN_BRACKET_ESCAPE | IN_NUMERIC_ESCAPE | IN_ESCAPE);
        }
    } else {
        ESH_INSTANCE->flags
            &= ~(IN_BRACKET_ESCAPE | IN_NUMERIC_ESCAPE | IN_ESCAPE);
    }

    switch (esc) {
    case ESCCHAR_UP:
    case ESCCHAR_DOWN:
        if (esc == ESCCHAR_UP) {
            ++ESH_INSTANCE->hist.idx;
        } else if (ESH_INSTANCE->hist.idx) {
            --ESH_INSTANCE->hist.idx;
        }
        if (ESH_INSTANCE->hist.idx) {
            int offset = esh_hist_nth(ESH_INSTANCE,
                    ESH_INSTANCE->hist.idx - 1);
            if (offset >= 0 || esc == ESCCHAR_DOWN) {
                esh_hist_print(ESH_INSTANCE, offset);
            } else if (esc == ESCCHAR_UP) {
                // Don't overscroll the top
                --ESH_INSTANCE->hist.idx;
            }
        } else {
            esh_restore(ESH_INSTANCE);
        }
        break;

    case ESCCHAR_LEFT:
        cdelta = -1;
        goto cmove;
    case ESCCHAR_RIGHT:
        cdelta = 1;
        goto cmove;
    case ESCCHAR_HOME:
        cdelta = -ESH_INSTANCE->ins;
        goto cmove;
    case ESCCHAR_END:
        cdelta = ESH_INSTANCE->cnt - ESH_INSTANCE->ins;
        goto cmove;
    case ESCCHAR_CTRLLEFT:
        cdelta = -1;
        goto wmove;
    case ESCCHAR_CTRLRIGHT:
        cdelta = 1;
        goto wmove;
    }

    return;
cmove: // micro-optimization, yo!
    cursor_move(ESH_INSTANCE, cdelta);
    return;
wmove:
    word_move(ESH_INSTANCE, cdelta);
}


/**
 * Return whether the command in the edit buffer is a NOP and should be ignored.
 * This does not substitute the selected history item.
 */
static bool command_is_nop(struct esh * esh)
{
    int i;

    (void) esh;
    for (i = 0; ESH_INSTANCE->buffer[i]; ++i) {
        if (ESH_INSTANCE->buffer[i] != ' ') {
            return false;
        }
    }
    return true;
}


/**
 * Process the command in the buffer and give it to the command callback. If
 * the buffer has overflowed, call the overflow callback instead.
 */
static void execute_command(struct esh * esh)
{
    (void) esh;

    // If a command from the history is selected, put it in the edit buffer.
    esh_hist_substitute(ESH_INSTANCE);

    if (ESH_INSTANCE->cnt >= ESH_BUFFER_LEN) {
        do_overflow_callback(ESH_INSTANCE, ESH_INSTANCE->buffer);
        ESH_INSTANCE->cnt = ESH_INSTANCE->ins = 0;
        esh_print_prompt(ESH_INSTANCE);
        return;
    } else {
        ESH_INSTANCE->buffer[ESH_INSTANCE->cnt] = 0;
    }

    esh_putc(ESH_INSTANCE, '\n');

    if (!command_is_nop(ESH_INSTANCE)) {
        esh_hist_add(ESH_INSTANCE, ESH_INSTANCE->buffer);

        int argc = esh_parse_args(ESH_INSTANCE);

        if (argc > ESH_ARGC_MAX) {
            do_overflow_callback(ESH_INSTANCE, ESH_INSTANCE->buffer);
        } else if (argc > 0) {
            do_command(ESH_INSTANCE, argc, ESH_INSTANCE->argv);
        }
    }

    ESH_INSTANCE->cnt = ESH_INSTANCE->ins = 0;
    esh_print_prompt(ESH_INSTANCE);
}


void esh_print_prompt(struct esh * esh)
{
    (void) esh;
    esh_puts_flash(ESH_INSTANCE, FSTR(ESH_PROMPT));
}


/**
 * Default overflow callback. This just prints a message.
 */
// API WARNING: This function is separately declared in lib.rs
void esh_default_overflow(struct esh * esh, char const * buffer, void * arg)
{
    (void) esh;
    (void) buffer;
    (void) arg;
    esh_puts_flash(ESH_INSTANCE, FSTR("\nesh: command buffer overflow\n"));
}


bool esh_putc(struct esh * esh, char c)
{
    (void) esh;

    do_print_callback(ESH_INSTANCE, c);
    return false;
}


bool esh_puts(struct esh * esh, char const * s)
{
    (void) esh;
    char c;

    while ((c = *s++)) {
        esh_putc(ESH_INSTANCE, c);
    }
    return false;
}


#ifdef __AVR_ARCH__
bool esh_puts_flash(struct esh * esh, char const __flash * s)
{
    (void) esh;
    char c;

    while ((c = *s++)) {
        esh_putc(ESH_INSTANCE, c);
    }
    return false;
}
#endif // __AVR_ARCH__


void esh_restore(struct esh * esh)
{
    (void) esh;

    esh_puts_flash(ESH_INSTANCE, FSTR(ESC_ERASE_LINE "\r")); // Clear line
    esh_print_prompt(ESH_INSTANCE);
    ESH_INSTANCE->buffer[ESH_INSTANCE->cnt] = 0;
    esh_puts(ESH_INSTANCE, ESH_INSTANCE->buffer);
    term_cursor_move(ESH_INSTANCE,
            -(int)(ESH_INSTANCE->cnt - ESH_INSTANCE->ins));
}


#ifdef ESH_RUST
// API WARNING: This function is separately declared in lib.rs
size_t esh_get_slice_size(void)
{
    return sizeof (struct char_slice);
}
#endif


/**
 * Move only the terminal cursor. This does not move the insertion point.
 */
static void term_cursor_move(struct esh * esh, int n)
{
    (void) esh;

    for ( ; n > 0; --n) {
        esh_puts_flash(ESH_INSTANCE, FSTR(ESC_CURSOR_RIGHT));
    }

    for ( ; n < 0; ++n) {
        esh_puts_flash(ESH_INSTANCE, FSTR(ESC_CURSOR_LEFT));
    }
}


/**
 * Move the esh cursor. This applies history substitution, moves the terminal
 * cursor, and moves the insertion point.
 */
static void cursor_move(struct esh * esh, int n)
{
    (void) esh;

    esh_hist_substitute(ESH_INSTANCE);
    if ((int) ESH_INSTANCE->ins + n < 0) {
        n = -ESH_INSTANCE->ins;
    } else if ((int) ESH_INSTANCE->ins + n > (int) ESH_INSTANCE->cnt) {
        n = ESH_INSTANCE->cnt - ESH_INSTANCE->ins;
    }

    term_cursor_move(ESH_INSTANCE, n);
    ESH_INSTANCE->ins += n;
}


/**
 * Move the esh cursor backwards or forwards one word. This applies history
 * substitution, moves the terminal cursor, and moves the insertion point.
 *
 * @param dir - move forward if +1 or negative if -1. All other numbers produce
 *              undefined behavior.
 */
static void word_move(struct esh * esh, int dir)
{
    (void) esh;

    size_t ins = ESH_INSTANCE->ins;
    esh_hist_substitute(ESH_INSTANCE);

    if (dir == 0) {
        return;
    } else if (dir < 0) {
        for (; ins > 0 && ESH_INSTANCE->buffer[ins - 1] == ' '; --ins);
        for (; ins > 0 && ESH_INSTANCE->buffer[ins - 1] != ' '; --ins);
    } else {
        const size_t cnt = ESH_INSTANCE->cnt;
        for (; ins < cnt && ESH_INSTANCE->buffer[ins] != ' '; ++ins);
        for (; ins < cnt && ESH_INSTANCE->buffer[ins] == ' '; ++ins);
    }

    term_cursor_move(ESH_INSTANCE, ins - ESH_INSTANCE->ins);
    ESH_INSTANCE->ins = ins;
}


/**
 * Either insert or delete a character at the current insertion point.
 * @param esh - esh instance
 * @param c - character to insert, or 0 to delete
 */
static void ins_del(struct esh * esh, char c)
{
    (void) esh;

    int sgn = c ? 1 : -1;
    bool move = (ESH_INSTANCE->ins != ESH_INSTANCE->cnt);

    memmove(&ESH_INSTANCE->buffer[ESH_INSTANCE->ins + sgn],
            &ESH_INSTANCE->buffer[ESH_INSTANCE->ins],
            ESH_INSTANCE->cnt - ESH_INSTANCE->ins);

    if (c) {
        ESH_INSTANCE->buffer[ESH_INSTANCE->ins] = c;
    }

    ESH_INSTANCE->cnt += sgn;
    ESH_INSTANCE->ins += sgn;

    if (move) {
        esh_restore(ESH_INSTANCE);
    } else if (!c) {
        esh_puts_flash(ESH_INSTANCE, FSTR("\b \b"));
    } else {
        esh_putc(ESH_INSTANCE, c);
    }
}
