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
#include <esc/mem.h>
#include <esc/debug.h>
#include <esc/thread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define OCC_MAP_SIZE			512
#define PAGE_SIZE				4096

#ifdef __eco32__
#define DEBUG_ALLOC_N_FREE		0
#define DEBUG_ALLOC_N_FREE_PID	24	/* -1 = all */
/* TODO we need the alignment */
#define DEBUG_ADD_GUARDS		1
#elif DEBUGGING
#define DEBUG_ALLOC_N_FREE		0
#define DEBUG_ALLOC_N_FREE_PID	24	/* -1 = all */
#define DEBUG_ADD_GUARDS		1
#endif

#define GUARD_MAGIC				0xDEADBEEF
#define ALIGN(count,align)		(((count) + (align) - 1) & ~((align) - 1))

#if DEBUG_ADD_GUARDS
void *_malloc(size_t size);
void *_calloc(size_t num,size_t size);
void * _realloc(void *addr,size_t size);
void _free(void *addr);
#	define malloc_guard(size)			malloc(size)
#	define calloc_guard(num,size)		calloc(num,size)
#	define realloc_guard(addr,size)		realloc(addr,size)
#	define free_guard(addr)				free(addr)
#else
#	define _malloc(size)				malloc(size)
#	define _calloc(num,size)			calloc(num,size)
#	define _realloc(addr,size)			realloc(addr,size)
#	define _free(addr)					free(addr)
#endif

/* an area in memory */
typedef struct sMemArea sMemArea;
struct sMemArea {
	size_t size;
	void *address;
	sMemArea *next;
};

/* a linked list of free and usable areas. That means the areas have an address and size */
static sMemArea *usableList = NULL;
/* a linked list of free but not usable areas. That means the areas have no address and size */
static sMemArea *freeList = NULL;
/* a hashmap with occupied-lists, key is getHash(address) */
static sMemArea *occupiedMap[OCC_MAP_SIZE] = {NULL};
/* total number of pages we're using */
static size_t pageCount = 0;

/**
 * Allocates a new page for areas
 *
 * @return true on success
 */
static bool loadNewAreas(void);

/**
 * Allocates new space for alloation
 *
 * @param size the minimum size
 * @return true on success
 */
static bool loadNewSpace(size_t size);

/**
 * Calculates the hash for the given address that should be used as key in occupiedMap
 *
 * @param addr the address
 * @return the key
 */
static uint getHash(void *addr);

/* the lock for the heap */
static tULock mlock = 0;

#if DEBUG_ADD_GUARDS
void *malloc_guard(size_t size) {
	uint *a;
	size = ALIGN(size,sizeof(uint));
	a = (uint*)_malloc(size + sizeof(uint) * 3);
	if(a) {
		a[0] = GUARD_MAGIC;
		a[1] = size;
		a[size / sizeof(uint) + 2] = GUARD_MAGIC;
		return a + 2;
	}
	return NULL;
}

void *calloc_guard(size_t num,size_t size) {
	uint *a;
	size = ALIGN(num * size,sizeof(uint));
	a = (uint*)_malloc(size + sizeof(uint) * 3);
	if(a) {
		a[0] = GUARD_MAGIC;
		a[1] = size;
		a[size / sizeof(uint) + 2] = GUARD_MAGIC;
		memclear(a + 2,size);
		return a + 2;
	}
	return NULL;
}

void *realloc_guard(void *addr,size_t size) {
	uint *a = (uint*)addr;
	size = ALIGN(size,sizeof(uint));
	if(a) {
		assert(a[-2] == GUARD_MAGIC);
		assert(a[a[-1] / sizeof(uint)] == GUARD_MAGIC);
		a = _realloc(a - 2,size + sizeof(uint) * 3);
	}
	else
		a = _realloc(NULL,size + sizeof(uint) * 3);
	if(a) {
		a[0] = GUARD_MAGIC;
		a[1] = size;
		a[size / sizeof(uint) + 2] = GUARD_MAGIC;
		return a + 2;
	}
	return NULL;
}

void free_guard(void *addr) {
	uint *a = (uint*)addr;
	if(a) {
		assert(a[-2] == GUARD_MAGIC);
		assert(a[a[-1] / sizeof(uint)] == GUARD_MAGIC);
		_free(a - 2);
	}
}
#endif

void *_malloc(size_t size) {
	sMemArea *area,*prev,*narea;
	sMemArea **list;

	if(size == 0)
		return NULL;

	locku(&mlock);

	/* find a suitable area */
	prev = NULL;
	area = usableList;
	while(area != NULL) {
		if(area->size >= size)
			break;
		prev = area;
		area = area->next;
	}

	/* no area found? */
	if(area == NULL) {
		if(!loadNewSpace(size)) {
			unlocku(&mlock);
			return NULL;
		}
		/* we can assume that it fits */
		area = usableList;
		/* remove from usable-list */
		usableList = area->next;
	}
	else {
		/* remove from usable-list */
		if(prev == NULL)
			usableList = area->next;
		else
			prev->next = area->next;
	}

	/* is there space left? */
	if(size < area->size) {
		if(freeList == NULL) {
			if(!loadNewAreas()) {
				/* TODO we may have changed something... */
				unlocku(&mlock);
				return NULL;
			}
		}

		/* split the area */
		narea = freeList;
		freeList = freeList->next;
		narea->address = (void*)((uintptr_t)area->address + size);
		narea->size = area->size - size;
		area->size = size;
		/* insert in usable-list */
		narea->next = usableList;
		usableList = narea;
	}

#if DEBUG_ALLOC_N_FREE
	if(DEBUG_ALLOC_N_FREE_PID == -1 || getpid() == DEBUG_ALLOC_N_FREE_PID) {
		size_t i = 0,*trace = getStackTrace();
		debugf("[A] %x %d ",area->address,area->size);
		while(*trace && i++ < 10) {
			debugf("%x",*trace);
			if(trace[1])
				debugf(" ");
			trace++;
		}
		debugf("\n");
	}
#endif

	/* insert in occupied-map */
	list = occupiedMap + getHash(area->address);
	area->next = *list;
	*list = area;

	unlocku(&mlock);
	return area->address;
}

void *_calloc(size_t num,size_t size) {
	void *a = _malloc(num * size);
	if(a == NULL)
		return NULL;

	memclear(a,num * size);
	return a;
}

void _free(void *addr) {
	sMemArea *area,*a,*prev,*next,*oprev,*nprev,*pprev,*tprev;

	/* addr may be null */
	if(addr == NULL)
		return;

	locku(&mlock);

	/* find the area with given address */
	oprev = NULL;
	area = occupiedMap[getHash(addr)];
	while(area != NULL) {
		if(area->address == addr)
			break;
		oprev = area;
		area = area->next;
	}

	/* area not found? */
	if(area == NULL) {
		unlocku(&mlock);
		return;
	}

	/* find the previous and next free areas */
	prev = NULL;
	next = NULL;
	tprev = NULL;
	pprev = NULL;
	nprev = NULL;
	a = usableList;
	while(a != NULL) {
		if((uintptr_t)a->address + a->size == (uintptr_t)addr) {
			prev = a;
			pprev = tprev;
		}
		if((uintptr_t)a->address == (uintptr_t)addr + area->size) {
			next = a;
			nprev = tprev;
		}
		/* do we have both? */
		if(prev && next)
			break;
		tprev = a;
		a = a->next;
	}

	/* remove area from occupied-map */
	if(oprev)
		oprev->next = area->next;
	else
		occupiedMap[getHash(addr)] = area->next;

#if DEBUG_ALLOC_N_FREE
	if(DEBUG_ALLOC_N_FREE_PID == -1 || getpid() == DEBUG_ALLOC_N_FREE_PID) {
		size_t i = 0,*trace = getStackTrace();
		debugf("[F] %x %d ",area->address,area->size);
		while(*trace && i++ < 10) {
			debugf("%x",*trace);
			if(trace[1])
				debugf(" ");
			trace++;
		}
		debugf("\n");
	}
#endif

	/* see what we have to merge */
	if(prev && next) {
		/* merge prev & area & next */
		area->size += prev->size + next->size;
		area->address = prev->address;
		/* remove prev and next from the list */
		if(nprev)
			nprev->next = next->next;
		else
			usableList = next->next;
		/* special-case if next is the previous of prev */
		if(pprev == next) {
			if(nprev)
				nprev->next = prev->next;
			else
				usableList = prev->next;
		}
		else {
			if(pprev)
				pprev->next = prev->next;
			else
				usableList = prev->next;
		}
		/* put area on the usable-list */
		area->next = usableList;
		usableList = area;
		/* put prev and next on the freelist */
		prev->next = next;
		next->next = freeList;
		freeList = prev;
	}
	else if(prev) {
		/* merge preg & area */
		prev->size += area->size;
		/* put area on the freelist */
		area->next = freeList;
		freeList = area;
	}
	else if(next) {
		/* merge area & next */
		next->address = area->address;
		next->size += area->size;
		/* put area on the freelist */
		area->next = freeList;
		freeList = area;
	}
	else {
		/* insert area in usableList */
		area->next = usableList;
		usableList = area;
	}

	unlocku(&mlock);
}

void *_realloc(void *addr,size_t size) {
	sMemArea *area,*a,*prev;
	if(addr == NULL)
		return _malloc(size);

	locku(&mlock);

	/* find the area with given address */
	area = occupiedMap[getHash(addr)];
	while(area != NULL) {
		if(area->address == addr)
			break;
		area = area->next;
	}

	/* area not found? */
	if(area == NULL) {
		unlocku(&mlock);
		return NULL;
	}

	/* ignore size-shrinks */
	if(size < area->size) {
		unlocku(&mlock);
		return addr;
	}

	a = usableList;
	prev = NULL;
	while(a != NULL) {
		/* found the area behind? */
		if(a->address == (uchar*)area->address + area->size) {
			/* if the size of both is big enough we can use them */
			if(area->size + a->size >= size) {
				/* space left? */
				if(size < area->size + a->size) {
					/* so move the area forward */
					a->address = (void*)((uintptr_t)area->address + size);
					a->size = (area->size + a->size) - size;
				}
				/* otherwise we don't need a anymore */
				else {
					/* remove a from usable-list */
					if(prev == NULL)
						usableList = a->next;
					else
						prev->next = a->next;
					/* free a */
					a->next = freeList;
					freeList = a;
				}

				area->size = size;
				unlocku(&mlock);
				return addr;
			}

			/* makes no sense to continue since we've found the area behind */
			break;
		}
		prev = a;
		a = a->next;
	}

	unlocku(&mlock);

	/* the areas are not big enough, so allocate a new one */
	a = _malloc(size);
	if(a == NULL)
		return NULL;

	/* copy the old data and free it */
	memcpy(a,addr,area->size);
	_free(addr);
	return a;
}

static bool loadNewSpace(size_t size) {
	void *oldEnd;
	size_t count;
	sMemArea *area;

	/* no free areas? */
	if(freeList == NULL) {
		if(!loadNewAreas())
			return false;
	}

	/* check for overflow */
	if(size + PAGE_SIZE < PAGE_SIZE)
		return false;

	/* allocate the required pages */
	count = (size + PAGE_SIZE - 1) / PAGE_SIZE;
	oldEnd = changeSize(count);
	if(oldEnd == NULL)
		return false;

	pageCount += count;
	/* take one area from the freelist and put the memory in it */
	area = freeList;
	freeList = freeList->next;
	area->address = (void*)((uintptr_t)oldEnd * PAGE_SIZE);
	area->size = PAGE_SIZE * count;
	/* put area in the usable-list */
	area->next = usableList;
	usableList = area;
	return true;
}

static bool loadNewAreas(void) {
	sMemArea *area,*end;
	void *oldEnd;

	/* allocate one page for area-structs */
	oldEnd = changeSize(1);
	if(oldEnd == NULL)
		return false;

	/* determine start- and end-address */
	pageCount++;
	area = (sMemArea*)((uintptr_t)oldEnd * PAGE_SIZE);
	end = area + (PAGE_SIZE / sizeof(sMemArea));

	/* put all areas in the freelist */
	freeList = area;
	area++;
	while(area < end) {
		area->next = freeList;
		freeList = area;
		area++;
	}

	return true;
}

static uint getHash(void *addr) {
	/* the algorithm distributes the entries more equally in the occupied-map. */
	/* borrowed from java.util.HashMap :) */
	uint h = (uint)addr;
	h ^= (h >> 20) ^ (h >> 12);
	/* note that we can use & (a-1) since OCC_MAP_SIZE = 2^x */
	return (h ^ (h >> 7) ^ (h >> 4)) & (OCC_MAP_SIZE - 1);
}

size_t heapspace(void) {
	sMemArea *area;
	size_t c = 0;
	area = usableList;
	while(area != NULL) {
		c += area->size;
		area = area->next;
	}
	return c;
}

/* #### TEST/DEBUG FUNCTIONS #### */
#if DEBUGGING

void printheap(void) {
	sMemArea *area;
	size_t i;

	printf("PageCount=%d\n",pageCount);
	printf("UsableList:\n");
	area = usableList;
	while(area != NULL) {
		printf("\t0x%x: addr=0x%x, size=0x%x, next=0x%x\n",area,area->address,area->size,area->next);
		area = area->next;
	}

	/*printf("FreeList:\n");
	area = freeList;
	while(area != NULL) {
		printf("\t0x%x: addr=0x%x, size=0x%x, next=0x%x\n",area,area->address,area->size,area->next);
		area = area->next;
	}*/

	printf("OccupiedMap:\n");
	for(i = 0; i < OCC_MAP_SIZE; i++) {
		area = occupiedMap[i];
		if(area != NULL) {
			printf("\t%d:\n",i);
			while(area != NULL) {
				printf("\t\t0x%x: addr=0x%x, size=0x%x, next=0x%x\n",area,area->address,area->size,area->next);
				area = area->next;
			}
		}
	}
}

#endif