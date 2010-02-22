/**
 * $Id: ctmain.c 271 2009-08-29 15:08:30Z nasmussen $
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
#include <esc/fileio.h>
#include <esc/io.h>
#include <esc/signals.h>
#include <messages.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errors.h>
#include "game.h"

static void sigTimer(tSig sig,u32 data);
static void qerror(const char *msg,...);
static void quit(void);

static tFD keymap = -1;
static sKmData kmdata;
static u32 time = 0;

int main(void) {
	/* backup screen and stop vterm to read from keyboard */
	ioctl(STDOUT_FILENO,IOCTL_VT_BACKUP,NULL,0);
	ioctl(STDOUT_FILENO,IOCTL_VT_DIS_RDKB,NULL,0);

	keymap = open("/drivers/kmmanager",IO_READ | IO_WRITE);
	if(keymap < 0)
		qerror("Unable to open keymap-driver");

	if(!game_init()) {
		quit();
		exit(EXIT_FAILURE);
	}

	if(setSigHandler(SIG_INTRPT_TIMER,sigTimer) < 0)
		qerror("Unable to set sig-handler");

	game_tick(time);
	while(1) {
		if(wait(EV_DATA_READABLE) != ERR_INTERRUPTED) {
			while(!eof(keymap)) {
				if(read(keymap,&kmdata,sizeof(kmdata)) < 0)
					qerror("Unable to read from keymap");
				game_handleKey(kmdata.keycode,kmdata.modifier,kmdata.isBreak,kmdata.character);
			}
		}

		if(!game_tick(time))
			break;
	}

	quit();
	return EXIT_SUCCESS;
}

static void sigTimer(tSig sig,u32 data) {
	UNUSED(sig);
	UNUSED(data);
	time++;
}

static void qerror(const char *msg,...) {
	va_list ap;
	quit();
	va_start(ap,msg);
	vprinte(msg,ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}

static void quit(void) {
	game_deinit();
	close(keymap);
	ioctl(STDOUT_FILENO,IOCTL_VT_RESTORE,NULL,0);
	ioctl(STDOUT_FILENO,IOCTL_VT_EN_RDKB,NULL,0);
}