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

#include <sys/common.h>
#include <sys/task/proc.h>
#include <sys/vfs/vfs.h>
#include <sys/dbg/console.h>
#include <sys/dbg/cmd/ls.h>
#include <sys/mem/cache.h>
#include <esc/fsinterface.h>
#include <esc/endian.h>
#include <string.h>
#include <errno.h>

#define DIRE_SIZE		(sizeof(sDirEntry) - (MAX_NAME_LEN + 1))

static int cons_cmd_ls_read(pid_t pid,sFile *file,sDirEntry *e);

static ScreenBackup backup;

int cons_cmd_ls(size_t argc,char **argv) {
	pid_t pid = Proc::getRunning();
	Lines lines;
	sStringBuffer buf;
	sFile *file;
	sDirEntry e;
	int res;
	if(Console::isHelp(argc,argv) || argc != 2) {
		vid_printf("Usage: %s <dir>\n",argv[0]);
		vid_printf("\tUses the current proc to be able to access the real-fs.\n");
		vid_printf("\tSo, I hope, you know what you're doing ;)\n");
		return 0;
	}

	/* redirect prints */
	res = vfs_openPath(pid,VFS_READ,argv[1],&file);
	if(res < 0)
		return res;
	while((res = cons_cmd_ls_read(pid,file,&e)) > 0) {
		buf.dynamic = true;
		buf.len = 0;
		buf.size = 0;
		buf.str = NULL;
		prf_sprintf(&buf,"%d %s",e.nodeNo,e.name);
		if(buf.str) {
			lines.appendStr(buf.str);
			Cache::free(buf.str);
		}
		lines.newLine();
	}
	vfs_closeFile(pid,file);
	if(res < 0)
		return res;
	lines.endLine();

	/* view the lines */
	vid_backup(backup.screen,&backup.row,&backup.col);
	Console::viewLines(&lines);
	vid_restore(backup.screen,backup.row,backup.col);
	return 0;
}

static int cons_cmd_ls_read(pid_t pid,sFile *file,sDirEntry *e) {
	ssize_t res;
	size_t len;
	/* default way; read the entry without name first */
	if((res = vfs_readFile(pid,file,e,DIRE_SIZE)) < 0)
		return res;
	/* EOF? */
	if(res == 0)
		return 0;

	e->nameLen = le16tocpu(e->nameLen);
	e->recLen = le16tocpu(e->recLen);
	e->nodeNo = le32tocpu(e->nodeNo);
	len = e->nameLen;
	/* ensure that the name is short enough */
	if(len >= MAX_NAME_LEN)
		return -ENAMETOOLONG;

	/* now read the name */
	if((res = vfs_readFile(pid,file,e->name,len)) < 0)
		return res;

	/* if the record is longer, we have to skip the stuff until the next record */
	if(e->recLen - DIRE_SIZE > len) {
		len = (e->recLen - DIRE_SIZE - len);
		if((res = vfs_seek(pid,file,len,SEEK_CUR)) < 0)
			return res;
	}

	/* ensure that it is null-terminated */
	e->name[e->nameLen] = '\0';
	return 1;
}