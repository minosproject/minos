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

#define ESH_INTERNAL
#include "esh.h"
#define ESH_INTERNAL_INCLUDE
#include "esh_argparser.h"
#include "esh_internal.h"

#define DEST(esh) ((esh)->buffer)


/**
 * Consume a quoted string. The source string will be modified into the
 * destination string as follows:
 *
 * source:  " b"
 * dest:     b
 *
 * This is safe to use when the destination and source buffer are the same;
 * it will only ever contract the data, not expand it.
 */
static void consume_quoted(struct esh *esh, size_t *src_i, size_t *dest_i)
{
    (void) esh;
    char quote = ESH_INSTANCE->buffer[*src_i];

    for (++*src_i; *src_i < ESH_INSTANCE->cnt; ++*src_i) {
        char c = ESH_INSTANCE->buffer[*src_i];
        if (c == quote) {
            // End of quoted string
            break;
        } else {
            DEST(ESH_INSTANCE)[*dest_i] = c;
            ++*dest_i;
        }
    }
}


int esh_parse_args(struct esh *esh)
{
    size_t i;
    (void) esh;
    int argc = 0;
    bool last_was_space = true;
    size_t dest = 0;

    for (i = 0; i < ESH_INSTANCE->cnt; ++i) {
        if (ESH_INSTANCE->buffer[i] == ' ') {
            last_was_space = true;
            ESH_INSTANCE->buffer[dest] = 0;
            ++dest;
        } else {
            if (last_was_space) {
                if (argc < ESH_ARGC_MAX) {
                    ESH_INSTANCE->argv[argc] = &ESH_INSTANCE->buffer[dest];
                }
                ++argc;
            }
            if (ESH_INSTANCE->buffer[i] == '\'' || ESH_INSTANCE->buffer[i] == '\"') {
                consume_quoted(ESH_INSTANCE, &i, &dest);
            } else {
                ESH_INSTANCE->buffer[dest] = ESH_INSTANCE->buffer[i];
                ++dest;
            }
            last_was_space = false;
        }
    }
    ESH_INSTANCE->buffer[dest] = 0;
    ESH_INSTANCE->buffer[ESH_BUFFER_LEN] = 0;
    return argc;
}
