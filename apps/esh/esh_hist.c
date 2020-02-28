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

// History allocation types
#define STATIC 1
#define MANUAL 2
#define MALLOC 3

#include "esh.h"
#define ESH_INTERNAL_INCLUDE
#include "esh_internal.h"
#include <minos/string.h>

#ifdef ESH_HIST_ALLOC
// Begin actual history implementation

/**
 * Initialize the history buffer.
 *
 * The initial value is a NUL byte followed by a fill of 0xff. This avoids
 * the extra empty-string history entry that would be seen if the buffer
 * were filled with 0x00.
 */
static void init_buffer(char * buffer)
{
    memset(buffer, 0xff, ESH_HIST_LEN);
    buffer[0] = 0;
}

/**
 * True signed modulo, for wraparound signed arithmetic. This is mathematically
 * equivalent to "0 + a   mod b".
 */
static int modulo(int n, int modulus)
{
    int rem = n % modulus;
    return (rem >= 0) ? rem : rem + modulus;
}

/**
 * Given an offset in the ring buffer, call the callback once for each
 * character in the string starting there. This is meant to abstract away
 * ring buffer access.
 *
 * @param esh - esh instance
 * @param offset - offset into the ring buffer
 * @param callback - will be called once per character
 *      - callback:param esh - esh instance
 *      - callback:param c - character
 *      - callback:return - true to stop iterating, false to continue
 *
 * Regardless of the callback's return value, iteration will always stop at NUL
 * or if the loop wraps all the way around.
 */
static void for_each_char(struct esh * esh, int offset,
        bool (*callback)(struct esh * esh, char c))
{
    int i;
    (void) esh;

    for (i = offset; ESH_INSTANCE->hist.hist[i]; i = (i + 1) % ESH_HIST_LEN) {
        if (i == modulo(offset - 1, ESH_HIST_LEN)) {
            // Wrapped around and didn't encounter NUL. Stop here to prevent
            // an infinite loop.
            return;
        }

        if (callback(ESH_INSTANCE, ESH_INSTANCE->hist.hist[i])) {
            return;
        }
    }
}


/**
 * Internal callback passed to for_each_char by clobber_buffer
 */
static bool clobber_cb(struct esh * esh, char c)
{
    (void) esh;
    ESH_INSTANCE->buffer[ESH_INSTANCE->cnt] = c;
    ++ESH_INSTANCE->cnt;
    ++ESH_INSTANCE->ins;
    return false;
}


/**
 * Put the selected history item in the buffer. Make sure to call
 * esh_restore afterward to display the buffer.
 * @param esh - esh instance
 * @param offset - offset into the ring buffer
 */
static void clobber_buffer(struct esh * esh, int offset)
{
    (void) esh;
    if (offset < 0 || offset >= ESH_HIST_LEN) {
        return;
    }

    ESH_INSTANCE->cnt = 0;
    ESH_INSTANCE->ins = 0;
    for_each_char(ESH_INSTANCE, offset, &clobber_cb);
}


bool esh_hist_init(struct esh * esh)
{
    (void) esh;
#if ESH_HIST_ALLOC == STATIC
    static char esh_hist[ESH_HIST_LEN] = {0};
    ESH_INSTANCE->hist.hist = &esh_hist[0];
    init_buffer(ESH_INSTANCE->hist.hist);
    return false;
#elif ESH_HIST_ALLOC == MALLOC
    ESH_INSTANCE->hist.hist = malloc(ESH_HIST_LEN);
    if (ESH_INSTANCE->hist.hist) {
        init_buffer(ESH_INSTANCE->hist.hist);
        return false;
    } else {
        return true;
    }
#elif ESH_HIST_ALLOC == MANUAL
    ESH_INSTANCE->hist.hist = NULL;
    return false;
#endif
}


int esh_hist_nth(struct esh * esh, int n)
{
    int i;
    (void) esh;
    const int start = modulo(ESH_INSTANCE->hist.tail - 1, ESH_HIST_LEN);
    const int stop = (ESH_INSTANCE->hist.tail + 1) % ESH_HIST_LEN;

    for (i = start; i != stop; i = modulo(i - 1, ESH_HIST_LEN)) {
        if (n && ESH_INSTANCE->hist.hist[i] == 0) {
            --n;
        } else if (ESH_INSTANCE->hist.hist[i] == 0) {
            return (i + 1) % ESH_HIST_LEN;
        }
    }

    return -1;
}


bool esh_hist_add(struct esh * esh, char const * s)
{
    int i;
    (void) esh;
    const int start = (ESH_INSTANCE->hist.tail + 1) % ESH_HIST_LEN;

    for (i = start; ; i = (i + 1) % ESH_HIST_LEN)
    {
        if (i == modulo(ESH_INSTANCE->hist.tail - 1, ESH_HIST_LEN)) {
            // Wrapped around
            ESH_INSTANCE->hist.tail = 0;
            init_buffer(ESH_INSTANCE->hist.hist);
            return true;
        }

        ESH_INSTANCE->hist.hist[i] = *s;

        if (*s) {
            ++s;
        } else {
            ESH_INSTANCE->hist.tail = i;
            return false;
        }
    }
}


void esh_hist_print(struct esh * esh, int offset)
{
    (void) esh;
    // Clear the line
    esh_puts_flash(ESH_INSTANCE, FSTR(ESC_ERASE_LINE "\r"));

    esh_print_prompt(ESH_INSTANCE);

    if (offset >= 0) {
        for_each_char(ESH_INSTANCE, offset, esh_putc);
    }
}


bool esh_hist_substitute(struct esh * esh)
{
    (void) esh;
    if (ESH_INSTANCE->hist.idx) {
        int offset = esh_hist_nth(ESH_INSTANCE, ESH_INSTANCE->hist.idx - 1);
        clobber_buffer(ESH_INSTANCE, offset);
        esh_restore(ESH_INSTANCE);
        ESH_INSTANCE->hist.idx = 0;
        return true;
    } else {
        return false;
    }
}

#endif // ESH_HIST_ALLOC

#if defined(ESH_HIST_ALLOC) && ESH_HIST_ALLOC == MANUAL

void esh_set_histbuf(struct esh * esh, char * buffer)
{
    ESH_INSTANCE->hist.hist = buffer;
    init_buffer(ESH_INSTANCE->hist.hist);
}

#else // ESH_HIST_ALLOC == MANUAL

void esh_set_histbuf(struct esh * esh, char * buffer)
{
    (void) esh;
    (void) buffer;
}

#endif // ESH_HIST_ALLOC == MANUAL
