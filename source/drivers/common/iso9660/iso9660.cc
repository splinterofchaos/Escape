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

#include <esc/common.h>
#include <esc/debug.h>
#include <esc/io.h>
#include <esc/proc.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#include "iso9660.h"
#include "rw.h"
#include "dir.h"
#include "file.h"
#include "direcache.h"

int main(int argc,char *argv[]) {
	char fspath[MAX_PATH_LEN];
	if(argc != 3)
		error("Usage: %s <wait> <devicePath>",argv[0]);

	/* build fs device name */
	char *dev = strrchr(argv[2],'/');
	/* it might also be 'cdrom' */
	if(!dev)
		dev = argv[2] - 1;
	snprintf(fspath,sizeof(fspath),"/dev/iso9660-%s",dev + 1);

	ISO9660FileSystem *fs;
	/* if the device is provided, simply use it */
	if(strcmp(argv[2],"cdrom") != 0) {
		/* the backend has to be a block device */
		if(!isblock(argv[2]))
			error("'%s' is neither a block-device nor a regular file",argv[2]);

		fs = new ISO9660FileSystem(argv[2]);
	}
	/* otherwise try all possible ATAPI-drives */
	else {
		char path[SSTRLEN("/dev/cda1") + 1];
		FSUser u;
		u.uid = ROOT_UID;
		u.gid = ROOT_GID;
		u.pid = getpid();
		int i;
		for(i = 0; i < 4; i++) {
			snprintf(path,sizeof(path),"/dev/cd%c1",'a' + (int)i);
			try {
				fs = new ISO9660FileSystem(path);

				/* try to find our kernel. if we've found it, it's likely that the user wants to
				 * boot from this device. unfortunatly there doesn't seem to be an easy way
				 * to find out the real boot-device from GRUB */
				inode_t ino = fs->resolve(&u,"/boot/escape",O_RDONLY);
				if(ino >= 0)
					break;
			}
			catch(...) {
				// if the device isn't present, try the next one
				continue;
			}
		}

		/* not found? */
		if(i >= 4)
			error("Unable to find cd-device with /boot/escape on it");
	}

	FSDevice fsdev(fs,fspath);
	fsdev.loop();
	return 0;
}

bool ISO9660FileSystem::ISO9660BlockCache::readBlocks(void *buffer,block_t start,size_t blockCount) {
	return ISO9660RW::readBlocks(_fs,buffer,start,blockCount);
}
bool ISO9660FileSystem::ISO9660BlockCache::writeBlocks(const void *,size_t,size_t) {
	return false;
}

ISO9660FileSystem::ISO9660FileSystem(const char *device)
		: FileSystem(), fd(::open(device,O_RDONLY)), primary(), dummy(initPrimaryVol(this,device)),
		  dirCache(this), blockCache(this) {
}

int ISO9660FileSystem::initPrimaryVol(ISO9660FileSystem *fs,const char *device) {
	/* better do that here in order to not construct the dircache and so on */
	if(fs->fd < 0)
		VTHROWE("Unable to open device '" << device << "'",fs->fd);

	/* read volume descriptors */
	for(int i = 0; ; i++) {
		if(ISO9660RW::readSectors(fs,&fs->primary,ISO_VOL_DESC_START + i,1) != 0)
			error("Unable to read volume descriptor from device");

		/* check identifier */
		if(strncmp(fs->primary.identifier,"CD001",sizeof(fs->primary.identifier)) != 0) {
			error("Identifier of primary volume invalid (%02x:%02x:%02x:%02x:%02x)",
				fs->primary.identifier[0],fs->primary.identifier[1],fs->primary.identifier[2],
				fs->primary.identifier[3],fs->primary.identifier[4]);
		}
		/* we need just the primary one */
		if(fs->primary.type == ISO_VOL_TYPE_PRIMARY)
			break;
	}
	return 0;
}

inode_t ISO9660FileSystem::resolve(FSUser *u,const char *path,uint flags) {
	return ISO9660Dir::resolve(this,u,path,flags);
}

inode_t ISO9660FileSystem::open(FSUser *,inode_t ino,uint) {
	/* nothing to do */
	return ino;
}

void ISO9660FileSystem::close(inode_t) {
	/* nothing to do */
}

int ISO9660FileSystem::stat(inode_t ino,sFileInfo *info) {
	time_t ts;
	const ISOCDirEntry *e = dirCache.get(ino);
	if(e == NULL)
		return -ENOBUFS;

	ts = dirDate2Timestamp(&e->entry.created);
	info->accesstime = ts;
	info->modifytime = ts;
	info->createtime = ts;
	info->blockCount = e->entry.extentSize.littleEndian / blockSize();
	info->blockSize = blockSize();
	info->device = 0;
	info->uid = 0;
	info->gid = 0;
	info->inodeNo = e->id;
	info->linkCount = 1;
	/* readonly here */
	if(e->entry.flags & ISO_FILEFL_DIR)
		info->mode = S_IFDIR | 0555;
	else
		info->mode = S_IFREG | 0555;
	info->size = e->entry.extentSize.littleEndian;
	return 0;
}

ssize_t ISO9660FileSystem::read(inode_t inodeNo,void *buffer,off_t offset,size_t count) {
	return ISO9660File::read(this,inodeNo,buffer,offset,count);
}

ssize_t ISO9660FileSystem::write(inode_t,const void *,off_t,size_t) {
	return -EROFS;
}

int ISO9660FileSystem::link(FSUser *,inode_t,inode_t,const char *) {
	return -EROFS;
}

int ISO9660FileSystem::unlink(FSUser *,inode_t,const char *) {
	return -EROFS;
}

int ISO9660FileSystem::mkdir(FSUser *,inode_t,const char *) {
	return -EROFS;
}

int ISO9660FileSystem::rmdir(FSUser *,inode_t,const char *) {
	return -EROFS;
}

int ISO9660FileSystem::chmod(FSUser *,inode_t,mode_t) {
	return -EROFS;
}

int ISO9660FileSystem::chown(FSUser *,inode_t,uid_t,gid_t) {
	return -EROFS;
}

void ISO9660FileSystem::sync() {
	/* nothing to do */
}

time_t ISO9660FileSystem::dirDate2Timestamp(const ISODirDate *ddate) {
	return timeof(ddate->month - 1,ddate->day - 1,ddate->year,
			ddate->hour,ddate->minute,ddate->second);
}

void ISO9660FileSystem::print(FILE *f) {
	ISOVolDesc *desc = &primary;
	ISOVolDate *date;
	fprintf(f,"Identifier: %.5s\n",desc->identifier);
	fprintf(f,"Size: %zu bytes\n",desc->data.primary.volSpaceSize.littleEndian * blockSize());
	fprintf(f,"Blocksize: %zu bytes\n",blockSize());
	fprintf(f,"SystemIdent: %.32s\n",desc->data.primary.systemIdent);
	fprintf(f,"VolumeIdent: %.32s\n",desc->data.primary.volumeIdent);
	date = &desc->data.primary.created;
	fprintf(f,"Created: %.4s-%.2s-%.2s %.2s:%.2s:%.2s\n",
			date->year,date->month,date->day,date->hour,date->minute,date->second);
	date = &desc->data.primary.modified;
	fprintf(f,"Modified: %.4s-%.2s-%.2s %.2s:%.2s:%.2s\n",
			date->year,date->month,date->day,date->hour,date->minute,date->second);
	fprintf(f,"Block cache:\n");
	blockCache.printStats(f);
	fprintf(f,"Directory entry cache:\n");
	dirCache.print(f);
}

#if DEBUGGING

void ISO9660FileSystem::printPathTbl() {
	size_t tblSize = primary.data.primary.pathTableSize.littleEndian;
	size_t secCount = (tblSize + ATAPI_SECTOR_SIZE - 1) / ATAPI_SECTOR_SIZE;
	ISOPathTblEntry *pe = static_cast<ISOPathTblEntry*>(malloc(ROUND_UP(tblSize,ATAPI_SECTOR_SIZE)));
	ISOPathTblEntry *start = pe;
	if(ISO9660RW::readSectors(this,pe,primary.data.primary.lPathTblLoc,secCount) != 0) {
		free(start);
		return;
	}
	printf("Path-Table:\n");
	while((uintptr_t)pe < ((uintptr_t)start + tblSize)) {
		printf("	length: %u\n",pe->length);
		printf("	extentLoc: %u\n",pe->extentLoc);
		printf("	parentTblIndx: %u\n",pe->parentTblIndx);
		printf("	name: %s\n",pe->name);
		printf("---\n");
		if(pe->length % 2 == 0)
			pe = (ISOPathTblEntry*)((uintptr_t)pe + sizeof(ISOPathTblEntry) + pe->length);
		else
			pe = (ISOPathTblEntry*)((uintptr_t)pe + sizeof(ISOPathTblEntry) + pe->length + 1);
	}
	free(start);
}

void ISO9660FileSystem::printTree(size_t extLoc,size_t extSize,size_t layer) {
	size_t i;
	ISODirEntry *e;
	ISODirEntry *content = (ISODirEntry*)malloc(extSize);
	if(content == NULL)
		return;
	if(ISO9660RW::readSectors(this,content,extLoc,extSize / ATAPI_SECTOR_SIZE) != 0)
		return;

	e = content;
	while((uintptr_t)e < (uintptr_t)content + extSize) {
		char bak;
		if(e->length == 0)
			break;
		for(i = 0; i < layer; i++)
			printf("  ");
		if(e->name[0] == ISO_FILENAME_THIS)
			printf("name: '.'\n");
		else if(e->name[0] == ISO_FILENAME_PARENT)
			printf("name: '..'\n");
		else {
			bak = e->name[e->nameLen];
			e->name[e->nameLen] = '\0';
			printf("name: '%s'\n",e->name);
			e->name[e->nameLen] = bak;
		}
		if((e->flags & ISO_FILEFL_DIR) && e->name[0] != ISO_FILENAME_PARENT &&
				e->name[0] != ISO_FILENAME_THIS) {
			printTree(e->extentLoc.littleEndian,e->extentSize.littleEndian,layer + 1);
		}
		e = (ISODirEntry*)((uintptr_t)e + e->length);
	}
	free(content);
}

void ISO9660FileSystem::printVolDesc(ISOVolDesc *desc) {
	printf("VolumeDescriptor @ %p\n",desc);
	printf("	version: 0x%02x\n",desc->version);
	printf("	identifier: %.5s\n",desc->identifier);
	switch(desc->type) {
		case ISO_VOL_TYPE_BOOTRECORD:
			printf("	type: bootrecord\n");
			printf("	bootSystemIdent: %.32s\n",desc->data.bootrecord.bootSystemIdent);
			printf("	bootIdent: %.32s\n",desc->data.bootrecord.bootIdent);
			break;
		case ISO_VOL_TYPE_PRIMARY:
			printf("	type: primary\n");
			printf("	systemIdent: %.32s\n",desc->data.primary.systemIdent);
			printf("	volumeIdent: %.32s\n",desc->data.primary.volumeIdent);
			printf("	volumeSetIdent: %.128s\n",desc->data.primary.volumeSetIdent);
			printf("	copyrightFile: %.38s\n",desc->data.primary.copyrightFile);
			printf("	abstractFile: %.36s\n",desc->data.primary.abstractFile);
			printf("	bibliographicFile: %.37s\n",desc->data.primary.bibliographicFile);
			printf("	applicationIdent: %.128s\n",desc->data.primary.applicationIdent);
			printf("	dataPreparerIdent: %.128s\n",desc->data.primary.dataPreparerIdent);
			printf("	publisherIdent: %.128s\n",desc->data.primary.publisherIdent);
			printf("	fileStructureVersion: %u\n",desc->data.primary.fileStructureVersion);
			printf("	lOptPathTblLoc: %u\n",desc->data.primary.lOptPathTblLoc);
			printf("	lPathTblLoc: %u\n",desc->data.primary.lPathTblLoc);
			printf("	mOptPathTblLoc: %u\n",desc->data.primary.mOptPathTblLoc);
			printf("	mPathTblLoc: %u\n",desc->data.primary.mPathTblLoc);
			printf("	logBlkSize: %u\n",desc->data.primary.logBlkSize.littleEndian);
			printf("	pathTableSize: %u\n",desc->data.primary.pathTableSize.littleEndian);
			printf("	volSeqNo: %u\n",desc->data.primary.volSeqNo.littleEndian);
			printf("	volSpaceSize: %u\n",desc->data.primary.volSpaceSize.littleEndian);
			printf("	created: ");
			printVolDate(&desc->data.primary.created);
			printf("\n");
			printf("	modified: ");
			printVolDate(&desc->data.primary.modified);
			printf("\n");
			printf("	expires: ");
			printVolDate(&desc->data.primary.expires);
			printf("\n");
			printf("	effective: ");
			printVolDate(&desc->data.primary.effective);
			printf("\n");
			break;
		case ISO_VOL_TYPE_PARTITION:
			printf("	type: partition\n");
			break;
		case ISO_VOL_TYPE_SUPPLEMENTARY:
			printf("	type: supplementary\n");
			break;
		case ISO_VOL_TYPE_TERMINATOR:
			printf("	type: terminator\n");
			break;
		default:
			printf("	type: *UNKNOWN*\n");
			break;
	}
	printf("\n");
}

void ISO9660FileSystem::printVolDate(ISOVolDate *date) {
	printf("%.4s-%.2s-%.2s %.2s:%.2s:%.2s.%.2s @%d",
			date->year,date->month,date->day,date->hour,date->minute,date->second,
			date->second100ths,date->offset);
}

#endif
