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
#include <esc/cmdargs.h>
#include <esc/io.h>
#include <esc/fileio.h>
#include <esc/dir.h>
#include <stdlib.h>
#include <string.h>
#include <fsinterface.h>

#define FS_NAME_LEN	12

typedef struct {
	u16 type;
	char name[FS_NAME_LEN];
} sFSType;

static sFSType types[] = {
	{FS_TYPE_EXT2,"ext2"}
};

int main(int argc,char *argv[]) {
	u32 i;
	u16 type;
	char rpath[MAX_PATH_LEN];
	char rdev[MAX_PATH_LEN];
	if(argc != 4 || isHelpCmd(argc,argv)) {
		fprintf(stderr,"Usage: %s <device> <path> <type>\n",argv[0]);
		fprintf(stderr,"	Types: ");
		for(i = 0; i < ARRAY_SIZE(types); i++)
			fprintf(stderr,"%s ",types[i].name);
		fprintf(stderr,"\n");
		return EXIT_FAILURE;
	}

	abspath(rdev,MAX_PATH_LEN,argv[1]);
	abspath(rpath,MAX_PATH_LEN,argv[2]);

	for(i = 0; i < ARRAY_SIZE(types); i++) {
		if(strcmp(types[i].name,argv[3]) == 0) {
			type = types[i].type;
			break;
		}
	}
	if(i >= ARRAY_SIZE(types)) {
		fprintf(stderr,"Unknown type '%s'\n",argv[3]);
		return EXIT_FAILURE;
	}

	if(mount(rdev,rpath,type) < 0) {
		printe("Unable to mount '%s' @ '%s' with type %d",rdev,rpath,type);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}