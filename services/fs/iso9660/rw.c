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
#include <esc/io.h>
#include <esc/fileio.h>
#include "iso9660.h"
#include "rw.h"

bool iso_rw_readBlocks(sISO9660 *h,void *buffer,u32 start,u16 blockCount) {
	return iso_rw_readSectors(h,buffer,ISO_BLKS_TO_SECS(h,start),ISO_BLKS_TO_SECS(h,blockCount));
}

bool iso_rw_readSectors(sISO9660 *h,void *buffer,u64 lba,u16 secCount) {
	if(seek(h->driverFd,lba * ATAPI_SECTOR_SIZE,SEEK_SET) < 0) {
		printe("Unable to seek to %x\n",lba * ATAPI_SECTOR_SIZE);
		return false;
	}
	if(read(h->driverFd,buffer,secCount * ATAPI_SECTOR_SIZE) != secCount * ATAPI_SECTOR_SIZE) {
		printe("Unable to read %d sectors @ %x\n",secCount,lba * ATAPI_SECTOR_SIZE);
		return false;
	}

	return true;
}