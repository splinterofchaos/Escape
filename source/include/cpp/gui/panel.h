/**
 * $Id$
 */

#ifndef PANEL_H_
#define PANEL_H_

#include <esc/common.h>
#include <gui/control.h>
#include <gui/event.h>
#include <gui/layout.h>
#include <vector>

namespace gui {
	class Window;

	/**
	 * A panel is a container for control-elements
	 */
	class Panel : public Control {
		friend class Window;
		friend class Control;

	private:
		static const Color DEF_BGCOLOR;

	public:
		Panel(Layout *layout)
			: Control(0,0,0,0), _focus(NULL), _bgColor(DEF_BGCOLOR),
			  _controls(vector<Control*>()), _layout(layout) {
		};
		/**
		 * Constructor
		 *
		 * @param x the x-position
		 * @param y the y-position
		 * @param width the width
		 * @param height the height
		 */
		Panel(gpos_t x,gpos_t y,gsize_t width,gsize_t height)
			: Control(x,y,width,height), _focus(NULL), _bgColor(DEF_BGCOLOR),
			  _controls(vector<Control*>()), _layout(NULL) {
		};
		/**
		 * Clones the given panel
		 *
		 * @param p the panel
		 */
		Panel(const Panel &p)
			: Control(p), _focus(p._focus), _bgColor(p._bgColor),
			  _controls(p._controls), _layout(p._layout) {
			// TODO clone the controls!
		};
		/**
		 * Destructor
		 */
		virtual ~Panel() {
		};
		/**
		 * Assignment-operator
		 *
		 * @param p the panel
		 */
		Panel &operator=(const Panel &p) {
			if(this == &p)
				return *this;
			_focus = p._focus;
			_bgColor = p._bgColor;
			// TODO clone the controls!
			_controls = p._controls;
			_layout = p._layout;
			return *this;
		};

		virtual gsize_t getPreferredWidth() const {
			return _layout ? _layout->getPreferredWidth() : 0;
		};
		virtual gsize_t getPreferredHeight() const {
			return _layout ? _layout->getPreferredHeight() : 0;
		};

		/**
		 * Sets the used layout for this panel. This is only possible if no controls have been
		 * added yet.
		 *
		 * @param l the layout
		 */
		inline void setLayout(Layout *l) {
			if(_controls.size() > 0) {
				throw logic_error("This panel does already have controls;"
						" you can't change the layout afterwards");
			}
			_layout = l;
		};

		/**
		 * @return the background-color
		 */
		inline Color getBGColor() const {
			return _bgColor;
		};
		/**
		 * Sets the background-color
		 *
		 * @param bg the background-color
		 */
		inline void setBGColor(Color bg) {
			_bgColor = bg;
		};
		
		/**
		 * The event-callbacks
		 *
		 * @param e the event
		 */
		virtual void onMousePressed(const MouseEvent &e);

		/**
		 * Paints the panel
		 *
		 * @param g the graphics-object
		 */
		virtual void paint(Graphics &g);
		/**
		 * Adds the given control to this panel
		 *
		 * @param c the control
		 * @param pos the position-specification for the layout
		 */
		void add(Control &c,Layout::pos_type pos = 0);

	protected:
		virtual void resizeTo(gsize_t width,gsize_t height);
		virtual void moveTo(gpos_t x,gpos_t y);

	private:
		/**
		 * @return the control that has the focus (not a panel!) or NULL if no one
		 */
		virtual const Control *getFocus() const {
			if(_focus)
				return _focus->getFocus();
			return NULL;
		};
		virtual Control *getFocus() {
			if(_focus)
				return _focus->getFocus();
			return NULL;
		};

		void setFocus(Control *c) {
			_focus = c;
		};

	protected:
		Control *_focus;
		Color _bgColor;
		vector<Control*> _controls;
		Layout *_layout;
	};

	ostream &operator<<(ostream &s,const Panel &p);
}

#endif /* PANEL_H_ */