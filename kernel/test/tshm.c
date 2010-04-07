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

#include <common.h>
#include <mem/kheap.h>
#include <mem/sharedmem.h>
#include <mem/vmm.h>
#include <mem/pmem.h>
#include <video.h>
#include <test.h>
#include "tshm.h"

/* forward declarations */
static void test_shm(void);
static void test_1(void);
static void test_2(void);

/* our test-module */
sTestModule tModShm = {
	"Shared memory",
	&test_shm
};
static u32 framesBefore;
static u32 heapBefore;
static u32 framesAfter;
static u32 heapAfter;

static void test_init(void) {
	heapBefore = kheap_getFreeMem();
	framesBefore = mm_getFreeFrmCount(MM_DEF);
}
static void test_finish(void) {
	heapAfter = kheap_getFreeMem();
	framesAfter = mm_getFreeFrmCount(MM_DEF);
	test_assertUInt(heapAfter,heapBefore);
	test_assertUInt(framesAfter,framesBefore);
}

static void test_shm(void) {
	test_1();
	test_2();
}

static void test_1(void) {
	sProc *p = proc_getRunning();
	test_caseStart("Testing shm_create() & shm_destroy()");

	test_init();
	test_assertTrue(shm_create(p,"myshm",3) >= 0);
	test_assertTrue(shm_destroy(p,"myshm") == 0);
	test_finish();

	test_caseSucceded();
}

static void test_2(void) {
	sProc *p = proc_getRunning();
	sProc *child1,*child2;
	tPid pid1,pid2;
	tVMRegNo reg1,reg2;
	test_caseStart("Testing shm_join() & shm_leave() & shm_remProc()");

	pid1 = proc_getFreePid();
	test_assertInt(proc_clone(pid1,false),0);
	child1 = proc_getByPid(pid1);
	pid2 = proc_getFreePid();
	test_assertInt(proc_clone(pid2,false),0);
	child2 = proc_getByPid(pid2);

	/* create dummy-regions to force vmm to extend the regions-array. this way we can check
	 * wether all memory is freed correctly */
	reg1 = vmm_add(child1,NULL,0,PAGE_SIZE,REG_SHM);
	test_assertTrue(reg1 >= 0);
	reg2 = vmm_add(child2,NULL,0,PAGE_SIZE,REG_SHM);
	test_assertTrue(reg2 >= 0);

	test_init();
	test_assertTrue(shm_create(p,"myshm",3) >= 0);
	test_assertTrue(shm_join(child1,"myshm") >= 0);
	test_assertTrue(shm_join(child2,"myshm") >= 0);
	test_assertTrue(shm_leave(child1,"myshm") >= 0);
	test_assertTrue(shm_leave(child2,"myshm") >= 0);
	test_assertTrue(shm_destroy(p,"myshm") == 0);
	test_finish();

	test_init();
	test_assertTrue(shm_create(p,"myshm",3) >= 0);
	test_assertTrue(shm_join(child1,"myshm") >= 0);
	test_assertTrue(shm_join(child2,"myshm") >= 0);
	test_assertTrue(shm_destroy(p,"myshm") == 0);
	test_finish();

	test_init();
	test_assertTrue(shm_create(p,"myshm",6) >= 0);
	test_assertTrue(shm_join(child1,"myshm") >= 0);
	test_assertTrue(shm_join(child2,"myshm") >= 0);
	shm_remProc(child2);
	shm_remProc(p);
	test_finish();

	vmm_remove(child2,reg2);
	vmm_remove(child1,reg1);
	proc_kill(child1);
	proc_kill(child2);

	test_caseSucceded();
}