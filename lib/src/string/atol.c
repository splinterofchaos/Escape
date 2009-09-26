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

#include <stddef.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>

s64 atol(const char *str) {
	s64 i = 0;
	bool neg = false;
	char c;

	vassert(str != NULL,"str == NULL");

	/* skip leading whitespace */
	while(isspace(*str))
		str++;
	/* negative? */
	if(*str == '-') {
		neg = true;
		str++;
	}
	/* read number */
	while((c = *str) >= '0' && c <= '9') {
		i = i * 10 + c - '0';
		str++;
	}
	/* switch sign? */
	if(neg)
		i = -i;
	return i;
}