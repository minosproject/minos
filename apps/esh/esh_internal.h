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

#ifndef ESH_INTERNAL_H
#define ESH_INTERNAL_H

#include "esh_incl_config.h"
#include "esh_hist.h"
#include <minos/types.h>

struct tty;

/**
 * If we're building for Rust, we need to know the size of a &[u8] in order
 * to allocate space for it. This definition should be equivalent. Because the
 * internal representation of a slice has not been stabilized [1], this is not
 * guaranteed to remain constant in the future; the Rust bindings will check
 * sizeof(struct char_slice) against mem::size_of::<&[u8]>().
 *
 * [1] https://github.com/rust-lang/rust/issues/27751
 */

#ifdef ESH_RUST
struct char_slice {
    char *p;
    size_t sz;
};
#endif

/**
 * esh instance struct. This holds all of the state that needs to be saved
 * between calls to esh_rx().
 */
struct esh {
    /**
     * The config item ESH_BUFFER_LEN is only the number of characters to be
     * stored, not characters plus termination.
     */
    char buffer[ESH_BUFFER_LEN + 1];

    /**
     * The Rust bindings require space allocated for an argv array of &[u8],
     * which can share memory with C's char* array to save limited SRAM.
     */
#ifdef ESH_RUST
    union {
        char * argv[ESH_ARGC_MAX];
        struct char_slice rust_argv[ESH_ARGC_MAX];
    };
#else
    char * argv[ESH_ARGC_MAX];
#endif

    size_t cnt;             ///< Number of characters currently held in .buffer
    size_t ins;             ///< Position of the current insertion point
    uint8_t flags;          ///< State flags for escape sequence parser
    struct esh_hist hist;
#ifndef ESH_STATIC_CALLBACKS
    esh_cb_command cb_command;
    esh_cb_print print;
    esh_cb_overflow overflow;
#endif
    void *cb_command_arg;
    void *cb_print_arg;
    void *cb_overflow_arg;

    struct tty *tty;
};

/**
 * On AVR, a number of strings should be stored in and read from flash space.
 * Other architectures have linearized address spaces and don't require this.
 */
#ifdef __AVR_ARCH__
#   define FSTR(s) (__extension__({ \
            static const __flash char __c[] = (s); \
            &__c[0];}))
#   define AVR_ONLY(x) x
#else
#   define FSTR(s) (s)
#   define AVR_ONLY(x)
#endif // __AVR_ARCH__

/**
 * Print one character.
 * @return false (allows it to be an esh_hist_for_each_char callback)
 */
bool esh_putc(struct esh *esh, char c);

/**
 * @internal
 * Print a string located in RAM.
 */
bool esh_puts(struct esh *esh, char const *s);

/**
 * @internal
 * Print a string located in flash. On all but AVR this is an alias for
 * esh_puts().
 */
#ifdef __AVR_ARCH__
bool esh_puts_flash(struct esh *esh, char const __flash * s);
#else
#define esh_puts_flash esh_puts
#endif

/**
 * Print the prompt string
 */
void esh_print_prompt(struct esh *esh);

/**
 * Overwrite the prompt and restore the buffer.
 */
void esh_restore(struct esh *esh);

/**
 * Call the print callback. Wrapper to avoid ifdefs for static callback.
 */
void esh_do_print_callback(struct esh *esh, char c);

/**
 * Call the main callback. Wrapper to avoid ifdefs for static callback.
 */
void esh_do_callback(struct esh *esh, int argc, char **argv);

/**
 * Call the overflow callback. Wrapper to avoid ifdefs for the static
 * callback.
 */
void esh_do_overflow_callback(struct esh *esh, char const * buffer);

#ifdef ESH_RUST
/**
 * Return what we think the size of a Rust &[u8] slice is. This is used to
 * verify that the statically allocated slice array is long enough, and also
 * to make sure a linker error is produced if ESH_RUST wasn't enabled
 * (which would mean the slice array wasn't allocated at all).
 */
size_t esh_get_slice_size(void);
#endif

#define ESC_CURSOR_RIGHT    "\33[1C"
#define ESC_CURSOR_LEFT     "\33[1D"
#define ESC_ERASE_LINE      "\33[2K"

#define ESCCHAR_UP      'A'
#define ESCCHAR_DOWN    'B'
#define ESCCHAR_RIGHT   'C'
#define ESCCHAR_LEFT    'D'
#define ESCCHAR_HOME    'H'
#define ESCCHAR_END     'F'
#define ESCCHAR_CTRLLEFT    'd'
#define ESCCHAR_CTRLRIGHT   'c'

#if ESH_ALLOC == STATIC
extern struct esh g_esh_struct;
#define ESH_INSTANCE (&g_esh_struct)
#else
#define ESH_INSTANCE esh
#endif // ESH_ALLOC

#endif // ESH_INTERNAL_H
