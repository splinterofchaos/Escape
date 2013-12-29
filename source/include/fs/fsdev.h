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
#include <esc/fsinterface.h>
#include <stdio.h>

typedef struct {
	uid_t uid;
	gid_t gid;
	pid_t pid;
} sFSUser;

/* The handler for the functions of the filesystem */
typedef void *(*fFSInit)(const char *device,char **usedDev,int *errcode);
typedef void (*fFSDeinit)(void *h);
typedef inode_t (*fFSResPath)(void *h,sFSUser *u,const char *path,uint flags);
typedef inode_t (*fFSOpen)(void *h,sFSUser *u,inode_t ino,uint flags);
typedef void (*fFSClose)(void *h,inode_t ino);
typedef int (*fFSStat)(void *h,inode_t ino,sFileInfo *info);
typedef ssize_t (*fFSRead)(void *h,inode_t inodeNo,void *buffer,off_t offset,size_t count);
typedef ssize_t (*fFSWrite)(void *h,inode_t inodeNo,const void *buffer,off_t offset,size_t count);
typedef int (*fFSLink)(void *h,sFSUser *u,inode_t dstIno,inode_t dirIno,const char *name);
typedef int (*fFSUnlink)(void *h,sFSUser *u,inode_t dirIno,const char *name);
typedef int (*fFSMkDir)(void *h,sFSUser *u,inode_t dirIno,const char *name);
typedef int (*fFSRmDir)(void *h,sFSUser *u,inode_t dirIno,const char *name);
typedef int (*fFSChmod)(void *h,sFSUser *u,inode_t ino,mode_t mode);
typedef int (*fFSChown)(void *h,sFSUser *u,inode_t ino,uid_t uid,gid_t gid);
typedef void (*fFSSync)(void *h);
typedef void (*fFSPrint)(FILE *f,void *h);

/* all information about a filesystem */
typedef struct {
	int type;
	bool readonly;
	void *handle;
	fFSInit init;			/* required */
	fFSDeinit deinit;		/* required */
	fFSResPath resPath;		/* required */
	fFSOpen open;			/* required */
	fFSClose close;			/* required */
	fFSStat stat;			/* required */
	fFSPrint print;			/* required */
	fFSRead read;			/* optional */
	fFSWrite write;			/* optional */
	fFSLink link;			/* optional */
	fFSUnlink unlink;		/* optional */
	fFSMkDir mkdir;			/* optional */
	fFSRmDir rmdir;			/* optional */
	fFSChmod chmod;			/* optional */
	fFSChown chown;			/* optional */
	fFSSync sync;			/* optional */
} sFileSystem;

int fs_driverLoop(const char *name,const char *diskDev,const char *fsDev,sFileSystem *fs);
