/**
 * $Id$
 * Copyright (C) 2008 - 2014 Nils Asmussen
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

#include <sys/common.h>
#include <sys/cmdargs.h>
#include <sys/io.h>
#include <sys/mount.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void usage(const char *name) {
	fprintf(stderr,"Usage: %s [--ms <ms>] <path>\n",name);
	fprintf(stderr,"    By default, the current mountspace (/sys/proc/self/ms) will\n");
	fprintf(stderr,"    be used. This can be overwritten by specifying --ms <ms>.\n");
	exit(EXIT_FAILURE);
}

int main(int argc,const char *argv[]) {
	char *path = NULL;
	char *mspath = (char*)"/sys/proc/self/ms";

	int res = ca_parse(argc,argv,CA_NO_FREE,"ms=s =s*",&mspath,&path);
	if(res < 0) {
		printe("Invalid arguments: %s",ca_error(res));
		usage(argv[0]);
	}
	if(ca_hasHelp())
		usage(argv[0]);

	int ms = open(mspath,O_WRITE);
	if(ms < 0)
		error("Unable to open '%s' for writing",mspath);
	if(unmount(ms,path) < 0)
		error("Unable to unmount '%s' from mountspace '%s'",path,mspath);
	close(ms);
	return EXIT_SUCCESS;
}
