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
#include <sys/mem/pagedir.h>
#include <sys/mem/cache.h>
#include <sys/mem/virtmem.h>
#include <sys/task/proc.h>
#include <sys/vfs/vfs.h>
#include <sys/vfs/file.h>
#include <sys/vfs/node.h>
#include <sys/spinlock.h>
#include <string.h>
#include <errno.h>

VFSFile::VFSFile(pid_t pid,VFSNode *p,char *n,bool &success)
		: VFSNode(pid,n,FILE_DEF_MODE,success), dynamic(true), size(), pos(), data() {
	if(!success)
		return;
	append(p);
}

VFSFile::VFSFile(pid_t pid,VFSNode *p,char *n,void *buffer,size_t len,bool &success)
		: VFSNode(pid,n,FILE_DEF_MODE,success), dynamic(false), size(), pos(len), data(buffer) {
	if(!success)
		return;
	append(p);
}

VFSFile::~VFSFile() {
	if(dynamic)
		Cache::free(data);
	data = NULL;
}

off_t VFSFile::seek(A_UNUSED pid_t pid,off_t position,off_t offset,uint whence) const {
	switch(whence) {
		case SEEK_SET:
			return offset;
		case SEEK_CUR:
			/* we can't validate the position here because the content of virtual files may be
			 * generated on demand */
			return position + offset;
		default:
		case SEEK_END:
			return pos;
	}
}

int VFSFile::reserve(size_t newSize) {
	SpinLock::acquire(&lock);
	int res = doReserve(newSize);
	SpinLock::release(&lock);
	return res;
}

int VFSFile::doReserve(size_t newSize) {
	if(!dynamic)
		return -ENOTSUP;
	/* ensure that we allocate enough memory */
	if(newSize > MAX_VFS_FILE_SIZE)
		return -ENOMEM;
	newSize = MIN(MAX(newSize,size * 2),MAX_VFS_FILE_SIZE);
	void *newData = Cache::realloc(data,newSize);
	if(!newData)
		return -ENOMEM;
	data = newData;
	size = newSize;
	return 0;
}

size_t VFSFile::getSize(A_UNUSED pid_t pid) const {
	return pos;
}

ssize_t VFSFile::read(A_UNUSED pid_t pid,A_UNUSED OpenFile *file,USER void *buffer,
                      off_t offset,size_t count) {
	size_t byteCount = 0;
	SpinLock::acquire(&lock);
	if(data != NULL) {
		if(offset > pos)
			offset = pos;
		byteCount = MIN((size_t)(pos - offset),count);
		if(byteCount > 0) {
			Thread::addLock(&lock);
			memcpy(buffer,(uint8_t*)data + offset,byteCount);
			Thread::remLock(&lock);
		}
	}
	SpinLock::release(&lock);
	return byteCount;
}

ssize_t VFSFile::write(A_UNUSED pid_t pid,A_UNUSED OpenFile *file,USER const void *buffer,
                       off_t offset,size_t count) {
	/* need to create cache? */
	SpinLock::acquire(&lock);
	if(data == NULL || size < offset + count) {
		int res = doReserve(MAX(offset + count,VFS_INITIAL_WRITECACHE));
		if(res < 0) {
			SpinLock::release(&lock);
			return res;
		}
	}

	/* copy the data into the cache; this may segfault, which will leave the the state of the
	 * file as it was before, except that we've increased the buffer-size */
	Thread::addLock(&lock);
	memcpy((uint8_t*)data + offset,buffer,count);
	Thread::remLock(&lock);
	/* we have checked size for overflow. so it is ok here */
	pos = MAX(pos,(off_t)(offset + count));
	SpinLock::release(&lock);
	return count;
}
