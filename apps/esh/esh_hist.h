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

#ifndef ESH_HIST_H
#define ESH_HIST_H

#include <minos/types.h>

/*
 * esh history support. This provides either a full history implementation or
 * a placeholder, depending on whether history was enabled in configuration.
 * This allows the main esh code to not be conditionally compiled.
 */

struct esh;

#ifdef ESH_HIST_ALLOC
// Begin actual history implementation

struct esh_hist {
    char *hist;
    int tail;
    int idx;
};

/**
 * Initialize history.
 * @param esh - esh instance
 * @return true on error. Can only return error if the history buffer is to be
 * allocated on heap.
 */
bool esh_hist_init(struct esh *esh);

/**
 * Count back n strings from the current tail of the ring buffer and return the
 * index the string starts at.
 *
 * @param esh - esh instance
 * @param n - count to the nth string back, where 0 is the last string added
 * @return offset in the ring buffer where the nth string starts, or -1 if
 *  there are not n-1 strings in the buffer.
 */
int esh_hist_nth(struct esh *esh, int n);

/**
 * Add a string into the buffer. If the string doesn't fit, the buffer is
 * intentionally reset to avoid restoring a corrupted string later.
 * @param esh - esh instance
 * @param s - string to add
 * @return true iff the string didn't fit (this is destructive!)
 */
bool esh_hist_add(struct esh *esh, char const *s);

/**
 * Overwrite the prompt and print a history suggestion.
 * @param esh - esh instance
 * @param offset - offset into the ring buffer
 */
void esh_hist_print(struct esh *esh, int offset);

/**
 * If history is currently being browsed, substitute the selected history item
 * for the buffer and redraw the buffer for editing.
 * @param esh - esh instance
 * @return true iff the substitution was made (i.e. history was being browsed)
 */
bool esh_hist_substitute(struct esh *esh);

#else // ESH_HIST_ALLOC
// Begin placeholder implementation

struct esh_hist {
    size_t idx;
};

#define INL static inline __attribute__((always_inline))

INL bool esh_hist_init(struct esh *esh)
{
    (void) esh;
    return false;
}

INL int esh_hist_nth(struct esh *esh, int n)
{
    (void) esh;
    (void) n;
    return -1;
}

INL bool esh_hist_add(struct esh *esh, char const *s)
{
    (void) esh;
    (void) s;
    return true;
}

INL void esh_hist_for_each_char( struct esh *esh, int offset,
        bool (*callback)(struct esh * esh, char c))
{
    (void) esh;
    (void) offset;
    (void) callback;
}

INL void esh_hist_print(struct esh *esh, int offset)
{
    (void) esh;
    (void) offset;
}

INL void esh_hist_restore(struct esh *esh)
{
    (void) esh;
}

INL void esh_hist_clobber(struct esh *esh, int offset)
{
    (void) esh;
    (void) offset;
}

INL bool esh_hist_substitute(struct esh *esh)
{
    (void) esh;
    return false;
}

#undef INL

#endif // ESH_HIST_ALLOC

#endif // ESH_HIST_H
