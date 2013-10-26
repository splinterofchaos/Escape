/**
 * $Id$
 * Copyright (C) 2008 - 2011 Nils Asmussen
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

#pragma once

#include <esc/common.h>
#include <gui/control.h>

namespace gui {
	class Wrapper : public Control {
	public:
		Wrapper(std::shared_ptr<Control> ctrl)
			: Control(), _focus(), _ctrl(ctrl), _doingLayout() {
		}
		Wrapper(std::shared_ptr<Control> ctrl,const Pos &pos,const Size &size)
			: Control(pos,size), _focus(), _ctrl(ctrl), _doingLayout() {
		}

		virtual bool isDirty() const {
			return UIElement::isDirty() || _ctrl->isDirty();
		}

		virtual bool layout();
		virtual void print(std::ostream &os, bool rec = true, size_t indent = 0) const;

	protected:
		virtual bool resizeTo(const Size &size) {
			bool res = Control::resizeTo(size);
			res |= _ctrl->resizeTo(size);
			return res;
		}
		virtual bool moveTo(const Pos &pos) {
			bool res = Control::moveTo(pos);
			// don't move the control, its position is relative to us. just refresh the paint-region
			_ctrl->setRegion();
			return res;
		}
		virtual void setRegion() {
			Control::setRegion();
			_ctrl->setRegion();
		}

		virtual void setParent(UIElement *e) {
			Control::setParent(e);
			_ctrl->setParent(this);
		}

		virtual void layoutChanged();

		virtual Control *getFocus() {
			if(_focus)
				return _focus->getFocus();
			return nullptr;
		}

		virtual void setFocus(Control *c) {
			_focus = c;
			_parent->setFocus(c ? this : nullptr);
		}

	protected:
		Control *_focus;
		std::shared_ptr<Control> _ctrl;
		bool _doingLayout;
	};
}
