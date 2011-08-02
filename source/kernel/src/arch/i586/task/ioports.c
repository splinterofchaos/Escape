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
#include <sys/arch/i586/gdt.h>
#include <sys/arch/i586/task/ioports.h>
#include <sys/task/smp.h>
#include <sys/task/proc.h>
#include <sys/mem/cache.h>
#include <sys/video.h>
#include <errors.h>
#include <string.h>

void ioports_init(pid_t pid) {
	sProc *p = proc_getByPid(pid);
	p->archAttr.ioMap = NULL;
}

int ioports_request(pid_t pid,uint16_t start,size_t count) {
	sProc *p = proc_getByPid(pid);
	if(p->archAttr.ioMap == NULL) {
		p->archAttr.ioMap = (uint8_t*)cache_alloc(IO_MAP_SIZE / 8);
		if(p->archAttr.ioMap == NULL)
			return ERR_NOT_ENOUGH_MEM;
		/* mark all as disallowed */
		memset(p->archAttr.ioMap,0xFF,IO_MAP_SIZE / 8);
	}

	/* 0xF8 .. 0xFF is reserved */
	if(OVERLAPS(0xF8,0xFF + 1,start,start + count))
		return ERR_IOMAP_RESERVED;

	/* 0 means allowed */
	while(count-- > 0) {
		p->archAttr.ioMap[start / 8] &= ~(1 << (start % 8));
		start++;
	}

	tss_setIOMap(p->archAttr.ioMap,true);
	return 0;
}

bool ioports_handleGPF(pid_t pid) {
	sProc *p = proc_getByPid(pid);
	if(p->archAttr.ioMap != NULL && !tss_ioMapPresent()) {
		/* load it and give the process another try */
		tss_setIOMap(p->archAttr.ioMap,false);
		return true;
	}
	return false;
}

int ioports_release(pid_t pid,uint16_t start,size_t count) {
	sProc *p = proc_getByPid(pid);
	if(p->archAttr.ioMap == NULL)
		return ERR_IOMAP_NOT_PRESENT;

	/* 0xF8 .. 0xFF is reserved */
	if(OVERLAPS(0xF8,0xFF + 1,start,start + count))
		return ERR_IOMAP_RESERVED;

	/* 1 means disallowed */
	while(count-- > 0) {
		p->archAttr.ioMap[start / 8] |= 1 << (start % 8);
		start++;
	}

	tss_setIOMap(p->archAttr.ioMap,true);
	return 0;
}

void ioports_free(pid_t pid) {
	sProc *p = proc_getByPid(pid);
	if(p->archAttr.ioMap != NULL) {
		cache_free(p->archAttr.ioMap);
		p->archAttr.ioMap = NULL;
	}
}

/* #### TEST/DEBUG FUNCTIONS #### */
#if DEBUGGING

void ioports_print(const uint8_t *map) {
	size_t i,j,c = 0;
	vid_printf("Reserved IO-ports:\n\t");
	for(i = 0; i < IO_MAP_SIZE / 8; i++) {
		for(j = 0; j < 8; j++) {
			if(!(map[i] & (1 << j))) {
				vid_printf("%zx, ",i * 8 + j);
				if(++c % 10 == 0)
					vid_printf("\n\t");
			}
		}
	}
}

#endif
