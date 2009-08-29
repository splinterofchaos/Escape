/**
 * $Id$
 * Copyright (C) 2008 - 2009 Nils Asmussen
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef ESCCODES_H_
#define ESCCODES_H_

#include <types.h>

#define MAX_ESCC_LENGTH			16

#define ESCC_ARG_UNUSED			-1

/* the known escape-codes */
#define ESCC_INVALID			-1
#define ESCC_INCOMPLETE			-2
#define ESCC_MOVE_LEFT			0
#define ESCC_MOVE_RIGHT			1
#define ESCC_MOVE_UP			2
#define ESCC_MOVE_DOWN			3
#define ESCC_MOVE_HOME			4
#define ESCC_MOVE_LINESTART		5
#define ESCC_MOVE_LINEEND		6
#define ESCC_KEYCODE			7
#define ESCC_COLOR				8

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Determines the escape-code from the given string. Assumes that the last char is '\033' and that
 * the string is null-terminated. If the code is incomplete or invalid ESCC_INCOMPLETE or
 * ESCC_INVALID will be returned, respectivly.
 *
 * @param str the string; will be changed if a valid escape-code has been read to the char behind
 * 	the code.
 * @param n1 will be set to the first argument (ESCC_ARG_UNUSED if unused)
 * @param n2 will be set to the second argument (ESCC_ARG_UNUSED if unused)
 * @return the scanned escape-code (ESCC_*)
 */
s32 escc_get(const char **str,s32 *n1,s32 *n2);

#ifdef __cplusplus
}
#endif

#endif /* ESCCODES_H_ */