/**
 * esh - embedded shell
 * ====================
 *
 * *****************************************************************************
 * * PLEASE read ALL of this documentation (all comment blocks starting with a *
 * * double-asterisk **). esh is simple, but a number of things need to be     *
 * * addressed by every esh user.                                              *
 * *****************************************************************************
 *
 * esh is a lightweight command shell for embedded applications in C or rust,
 * small enough to be used for (and intended for) debug UART consoles on
 * microcontrollers. Features include line editing, automatic argument
 * tokenizing (including sh-like quoting), and an optional history ring buffer.
 *
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
 *
 * -----------------------------------------------------------------------------
 *
 * 1.   Rust users
 * 2.   Configuring esh
 * 2.1.     Line endings
 * 2.2.     Static callbacks
 * 2.3.     History (optional)
 * 3.   Compiling esh
 * 4.   Code documentation
 * 4.1.     Basic interface: initialization and input
 * 4.2.     Callback types and registration functions
 * 4.3.     Advanced functions
 *
 * -----------------------------------------------------------------------------
 *
 * 1. Rust users
 * =============
 *
 * The Rust API and configuration is different, so if you want to use esh with
 * Rust, see esh_rust/src/esh/lib.rs for separate documentation. None of this
 * documentation (including the definitions for `esh_config.h`) applies.
 *
 * 2. Configuring esh
 * ==================
 *
 * esh expects a file called `esh_config.h` to be on the quoted include path. It
 * should define the following:
 *
 *     #define ESH_PROMPT       "% "        // Prompt string
 *     #define ESH_BUFFER_LEN   200         // Maximum length of a command
 *     #define ESH_ARGC_MAX     10          // Maximum argument count
 *     #define ESH_ALLOC        STATIC      // How to allocate struct esh (or MALLOC)
 *
 * Then, to use esh, include `esh.h`, and initialize an esh instance with:
 *
 *     struct esh * esh = esh_init();
 *
 * Unless you're using static callbacks (see below), register your callbacks
 * with:
 *
 *     esh_register_command(esh, &command_callback);
 *     esh_register_print(esh, &print_callback);
 *
 *     // Optional, see the documentation for this function:
 *     esh_register_overflow(esh, &overflow_callback);
 *
 * Now, just begin receiving characters from your serial interface and feeding
 * them in with:
 *
 *     esh_rx(esh, c);
 *
 * 2.1. Line endings
 * -----------------
 *
 * Internally, esh uses strictly `\n` line endings. A great many IO sources use
 * different line endings; the user is responsible for translating them for esh.
 * In general, most raw-mode unix-like terminals will give `\r` from the
 * keyboard and require `\r\n` as output, so your input functions should
 * translate `\r` to `\n`, and your output function should insert `\r` before
 * `\n`.
 *
 * 2.2. Static callbacks
 * ---------------------
 *
 * If you're only using one esh instance, or all your esh instances use the
 * same callbacks, callbacks can be compiled in statically, saving a bit of code
 * space, runtime, and RAM from keeping and following the pointers. Add the
 * following to your `esh_config.h`:
 *
 *     #define ESH_STATIC_CALLBACKS
 *
 * Now, simply name your callback functions `ESH_PRINT_CALLBACK`,
 * `ESH_COMMAND_CALLBACK`, and `ESH_OVERFLOW_CALLBACK` (the overflow callback
 * is still optional), and the linker will find them. The esh_register_*
 * functions are not defined when ESH_STATIC_CALLBACKS is set.
 *
 * 2.3. History (optional)
 * -----------------------
 *
 * To enable the optional history, define the following in `esh_config.h`:
 *
 *     #define ESH_HIST_ALLOC   STATIC      // STATIC, MANUAL, or MALLOC
 *     #define ESH_HIST_LEN     512         // Length. Use powers of 2 for
 *                                          //   efficiency on arithmetic-weak
 *                                          //   devices.
 *
 * If you chose `MANUAL` allocation, call `esh_set_histbuf()` once you have
 * allocated your own buffer of length ESH_HIST_LEN:
 *
 *     esh_set_histbuf(esh, &buffer[0]);
 *
 * Manual allocation was created for one specific purpose: history buffer in
 * external SRAM on AVR (the compiler and allocator don't generally know about
 * external SRAM unless you jump through hoops). However, it's there for
 * whatever you like :)
 *
 * WARNING: static allocation is only valid when using a SINGLE esh instance.
 * Using multiple esh instances with static allocation is undefined and WILL
 * make demons fly out your nose.
 *
 * 3. Compiling esh
 * ================
 *
 * esh has no build script of its own; building it is trivial and it's meant to
 * be integrated directly into your project.
 *
 *  1. Put the `esh` subdirectory on the include path.
 *  2. Make sure `esh_config.h` is on the quoted include path (`-iquote`).
 *  3. Make sure selected C standard is one of `c99`, `c11`, `gnu99`, or
 *       `gnu11`. On AVR, use only `gnu99` or `gnu11` (esh on AVR uses the
 *       Named Address Spaces GNU extension).
 *  4. Include *all* esh C source files in the build (whether or not you used
 *       the feature - e.g. esh_hist.c).
 *
 * esh should compile quietly with most warning settings, including
 * `-Wall -Wextra -pedantic`.
 */

#ifndef ESH_H
#define ESH_H

#define ESH_INTERNAL_INCLUDE
#include "esh_incl_config.h"
#include "esh_hist.h"
#undef ESH_INTERNAL_INCLUDE
#include <minos/types.h>

struct esh;

/**
 * -----------------------------------------------------------------------------
 *
 * 4. Code documentation
 */

/**
 * -----------------------------------------------------------------------------
 * 4.1. Basic interface: initialization and input
 */

/*
 * Return a pointer to an initialized esh object. Must be called before
 * any other functions.
 *
 * See ESH_ALLOC in esh_config.h - this should be STATIC or MALLOC.
 * If STATIC, only a single instance can be used. esh_init() will return a
 * pointer to it on the first call, and all subsequent calls will return
 * NULL.
 *
 * @return esh instance, or NULL in the following cases:
 *  - using malloc to allocate either the esh struct itself or the history
 *      buffer, and malloc returns NULL.
 *  - using static allocation and esh has already been initialized.
 *  - whichever allocation method was chosen for ESH_HIST_ALLOC, if any,
 *      failed.
 */
struct esh *esh_init(void);

/**
 * Pass in a character that was received.
 */
void esh_rx(struct esh * esh, char c);

#ifndef ESH_STATIC_CALLBACKS
/**
 * -----------------------------------------------------------------------------
 * 4.2. Callback types and registration functions
 *
 * These only exist if ESH_STATIC_CALLBACKS is not defined.
 */

/**
 * Callback to handle commands.
 * @param argc - number of arguments, including the command name
 * @param argv - arguments
 * @param arg - arbitrary argument passed to esh_set_command_arg()
 */
typedef void (*esh_cb_command)(struct esh *esh,
		int argc, char **argv, void *arg);

/**
 * Callback to print a character.
 * @param esh - the esh instance calling
 * @param c - the character to print
 * @param arg - arbitrary argument passed to esh_set_print_arg()
 */
typedef void (*esh_cb_print)(struct esh * esh, char c, void *arg);

/**
 * Callback to notify about overflow.
 * @param esh - the esh instance calling
 * @param buffer - the internal buffer, NUL-terminated
 * @param arg - arbitrary argument passed to esh_set_overflow_arg()
 */
typedef void (*esh_cb_overflow)(struct esh *esh,
		char const *buffer, void *rg);

/**
 * Register a callback to execute a command.
 */
void esh_register_command(struct esh *esh, esh_cb_command callback);

/**
 * Register a callback to print a character.
 */
void esh_register_print(struct esh *esh, esh_cb_print callback);

/**
 * Register a callback to notify about overflow. Optional; esh has an internal
 * overflow handler. To reset to that, set the handler to NULL.
 */
void esh_register_overflow(struct esh *esh, esh_cb_overflow overflow);
#endif

/**
 * -----------------------------------------------------------------------------
 * 4.3. Advanced functions
 */

/**
 * Set the location of the history buffer, if ESH_HIST_ALLOC is defined and
 * set to MANUAL. If ESH_HIST_ALLOC is not defined or not set to MANUAL, this
 * is a no-op.
 */
void esh_set_histbuf(struct esh *esh, char *buffer);

/**
 * Set an argument to be given to the command callback. Default is NULL.
 */
void esh_set_command_arg(struct esh *esh, void *arg);

/**
 * Set an argument to be given to the print callback. Default is NULL.
 */
void esh_set_print_arg(struct esh * esh, void *arg);

/**
 * Set an argument to be given to the overflow callback. Default is NULL.
 */
void esh_set_overflow_arg(struct esh *esh, void *arg);

#endif // ESH_H
