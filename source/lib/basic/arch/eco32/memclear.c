/**
 * $Id: memclear.c 849 2010-10-04 11:04:51Z nasmussen $
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
#include <string.h>

void memclear(void *addr,size_t count) {
	uint *waddr;
	char *baddr = (char*)addr;
	while((uint)baddr & (sizeof(uint) - 1)) {
		*baddr++ = 0;
		count--;
	}
	waddr = (uint*)baddr;
	while(count >= sizeof(uint)) {
		*waddr++ = 0;
		count -= sizeof(uint);
	}
	baddr = (char*)waddr;
	while(count-- > 0)
		*baddr++ = 0;
}