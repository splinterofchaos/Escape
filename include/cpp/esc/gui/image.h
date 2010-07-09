/**
 * $Id: image.h 479 2010-02-07 11:27:21Z nasmussen $
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

#ifndef IMAGE_H_
#define IMAGE_H_

#include <esc/common.h>
#include <esc/gui/common.h>
#include <esc/gui/graphics.h>

namespace esc {
	namespace gui {
		class Image {
		public:
			Image() {};
			virtual ~Image() {};

			virtual tSize getWidth() const = 0;
			virtual tSize getHeight() const = 0;

			virtual void paint(Graphics &g,tCoord x,tCoord y) = 0;
		};
	}
}

#endif /* IMAGE_H_ */