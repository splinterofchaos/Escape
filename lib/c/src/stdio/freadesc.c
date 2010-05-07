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

#include <esc/common.h>
#include <esc/exceptions/io.h>
#include <esc/io/iofilestream.h>
#include <stdio.h>
#include <assert.h>

s32 freadesc(FILE *f,s32 *n1,s32 *n2,s32 *n3) {
	s32 res = 0;
	sIOStream *s = (sIOStream*)f;
	assert(s->in);
	TRY {
		res = s->in->readEsc(s->in,n1,n2,n3);
	}
	CATCH(IOException,e) {
		res = EOF;
	}
	ENDCATCH
	return res;
}