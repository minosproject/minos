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
#error "esh_incl_config.h is an internal header and should not be included by the user."
#endif // ESH_INTERNAL_INCLUDE

#ifndef ESH_INCL_CONFIG_H
#define ESH_INCL_CONFIG_H

#define STATIC 1
#define MANUAL 2
#define MALLOC 3
#include "esh_config.h"

#ifdef ESH_RUST
#define ESH_STATIC_CALLBACKS
#endif

#endif // ESH_INCL_CONFIG_H
