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

#ifndef ESH_INTERNAL_INCLUDE
#error "esh_argparser.h is an internal header and should not be included by the user."
#endif // ESH_INTERNAL_INCLUDE

#ifndef ESH_ARGPARSER_H
#define ESH_ARGPARSER_H

/**
 * Map the buffer to the argv array, and return argc. If argc exceeds the
 * maximum, the full buffer will still be processed; argument pointers will
 * just not be stored beyond the maximum. The number that would have been
 * stored is returned.
 *
 * Handles whitespace and quotes. The entire buffer is processed in place,
 * with any changes leaveing the length equal or shorter.
 *
 * Following is an example buffer before and after processing (# for NUL),
 * with pointers stored in argv[] marked with ^
 *
 * before: git   config user.name "My Name"
 * after:  git###config#user.name#My Name#
 * argv:   ^     ^      ^         ^
 *
 * Rearranging the buffer is necessary because quotes can occur in the
 * middle of arguments. For example:
 *
 * before: why" would you ever"'"'"do this??"
 * after:  why would you ever"do this??#
 * argv:   ^
 *
 */
int esh_parse_args(esh_t * esh);

#endif // ESH_ARGPARSER_H
