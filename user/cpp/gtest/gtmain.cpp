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
#include <esc/proc.h>
#include <esc/messages.h>
#include <esc/io.h>
#include <esc/gui/application.h>
#include <esc/gui/window.h>
#include <esc/gui/button.h>
#include <esc/gui/editable.h>
#include <esc/gui/combobox.h>
#include <stdlib.h>

using namespace esc::gui;

int main(void) {
	if(fork() == 0) {
		Application *app = Application::getInstance();
		Window w1("Fenster 1",100,100,400,300);
		Button b("Click me!!",10,10,80,20);
		Editable e(10,40,200,20);
		w1.add(b);
		w1.add(e);
		ComboBox cb(10,80,100,20);
		cb.addItem("Huhu");
		cb.addItem("Wer ist da?");
		cb.addItem("ich nicht :P");
		w1.add(cb);
		return app->run();
	}

	if(fork() == 0) {
		Application *app = Application::getInstance();
		Window w2("Fenster 2",250,250,150,200);
		return app->run();
	}

	if(fork() == 0) {
		Application *app = Application::getInstance();
		Window w3("Fenster 3",50,50,100,40);
		return app->run();
	}

	if(fork() == 0) {
		exec("file:/bin/guishell",NULL);
		exit(EXIT_FAILURE);
	}

	Application *app = Application::getInstance();
	Window w4("Fenster 4",180,90,200,100);
	return app->run();
}
