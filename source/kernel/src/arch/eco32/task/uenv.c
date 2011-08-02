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
#include <sys/task/uenv.h>
#include <sys/task/thread.h>
#include <sys/task/event.h>
#include <sys/mem/vmm.h>
#include <sys/mem/paging.h>
#include <string.h>
#include <errors.h>
#include <assert.h>

#define KEYBOARD_BASE		0xF0200000
#define KEYBOARD_CTRL		0
#define KEYBOARD_IEN		0x02

static void uenv_startSignalHandler(sThread *t,sIntrptStackFrame *stack,sig_t sig);
static void uenv_addArgs(sThread *t,sIntrptStackFrame *frame,uintptr_t tentryPoint,bool newThread);

void uenv_handleSignal(sIntrptStackFrame *stack) {
	tid_t tid;
	sig_t sig;
	sThread *t = thread_getRunning();
	if((sig = thread_getSignal(t)) != SIG_COUNT)
		uenv_startSignalHandler(t,stack,sig);

	if(sig_hasSignal(&sig,&tid)) {
		if(t->tid == tid)
			uenv_startSignalHandler(t,stack,sig);
		else {
			t = thread_getById(tid);
			if(thread_setSignal(t,sig)) {
				ev_unblock(t);
				thread_switchTo(tid);
			}
		}
	}
}

int uenv_finishSignalHandler(sIntrptStackFrame *stack,sig_t signal) {
	uint32_t *regs;
	uint32_t *sp = (uint32_t*)stack->r[29];
	memcpy(stack->r,sp,REG_COUNT * sizeof(uint32_t));
	/* reenable device-interrupts */
	switch(signal) {
		case SIG_INTRPT_KB:
			regs = (uint32_t*)KEYBOARD_BASE;
			regs[KEYBOARD_CTRL] |= KEYBOARD_IEN;
			break;
		/* not necessary for disk here; the driver will reenable interrupts as soon as a new
		 * command is started */
	}
	return 0;
}

bool uenv_setupProc(const char *path,int argc,const char *args,size_t argsSize,
		const sStartupInfo *info,uintptr_t entryPoint) {
	UNUSED(path);
	UNUSED(argsSize);
	uint32_t *sp;
	char **argv;
	sThread *t = thread_getRunning();
	sIntrptStackFrame *frame = thread_getIntrptStack(t);

	/*
	 * Initial stack:
	 * +------------------+  <- top
	 * |     arguments    |
	 * |        ...       |
	 * +------------------+
	 * |       argv       |
	 * +------------------+
	 * |       argc       |
	 * +------------------+
	 *
	 * Registers:
	 * $4 = entryPoint (0 for initial thread, thread-entrypoint for others)
	 * $5 = TLSStart (0 if not present)
	 * $6 = TLSSize (0 if not present)
	 */

	/* get esp */
	thread_getStackRange(t,NULL,(uintptr_t*)&sp,0);

	/* copy arguments on the user-stack (4byte space) */
	sp--;
	argv = NULL;
	if(argc > 0) {
		char *str;
		int i;
		size_t len;
		argv = (char**)(sp - argc);
		/* space for the argument-pointer */
		sp -= argc;
		/* start for the arguments */
		str = (char*)sp;
		for(i = 0; i < argc; i++) {
			/* start <len> bytes backwards */
			len = strlen(args) + 1;
			str -= len;
			/* store arg-pointer and copy arg */
			argv[i] = str;
			memcpy(str,args,len);
			/* to next */
			args += len;
		}
		/* ensure that we don't overwrites the characters */
		sp = (uint32_t*)(((uintptr_t)str & ~(sizeof(uint32_t) - 1)) - sizeof(uint32_t));
	}

	/* store argc and argv */
	*sp-- = (uintptr_t)argv;
	*sp = argc;
	/* add TLS args and entrypoint; use prog-entry here because its always the entry of the
	 * program, not the dynamic-linker */
	uenv_addArgs(t,frame,info->progEntry,false);

	/* set entry-point and stack-pointer */
	frame->r[29] = (uint32_t)sp;
	frame->r[30] = entryPoint - 4; /* we'll skip the trap-instruction for syscalls */
	return true;
}

bool uenv_setupThread(const void *arg,uintptr_t tentryPoint) {
	uint32_t *sp;
	sThread *t = thread_getRunning();
	sIntrptStackFrame *frame = thread_getIntrptStack(t);

	/*
	 * Initial stack:
	 * +------------------+  <- top
	 * |       arg        |
	 * +------------------+
	 *
	 * Registers:
	 * $4 = entryPoint (0 for initial thread, thread-entrypoint for others)
	 * $5 = TLSStart (0 if not present)
	 * $6 = TLSSize (0 if not present)
	 */

	/* get esp */
	thread_getStackRange(t,NULL,(uintptr_t*)&sp,0);
	sp--;

	/* put arg on stack */
	*sp-- = (uintptr_t)arg;
	/* add TLS args and entrypoint */
	uenv_addArgs(t,frame,tentryPoint,true);

	/* set entry-point and stack-pointer */
	frame->r[29] = (uint32_t)sp;
	frame->r[30] = t->proc->entryPoint - 4; /* we'll skip the trap-instruction for syscalls */
	return true;
}

static void uenv_startSignalHandler(sThread *t,sIntrptStackFrame *stack,sig_t sig) {
	fSignal handler;
	uint32_t *sp = (uint32_t*)stack->r[29];

	/* if we've not entered the kernel by a trap, we have to decrease $30, because when returning
	 * from the signal, we'll always enter it by a trap, so that $30 will be increased */
	if(stack->irqNo != 20)
		stack->r[30] -= 4;

	thread_unsetSignal(t);
	handler = sig_startHandling(t->tid,sig);

	memcpy(sp - REG_COUNT,stack->r,REG_COUNT * sizeof(uint32_t));
	/* signal-number as arguments */
	stack->r[4] = sig;
	/* set new stack-pointer */
	stack->r[29] = (uint32_t)(sp - REG_COUNT);
	/* the process should continue here */
	stack->r[30] = (uint32_t)handler;
	/* and return here after handling the signal */
	stack->r[31] = t->proc->sigRetAddr;
}

static void uenv_addArgs(sThread *t,sIntrptStackFrame *frame,uintptr_t tentryPoint,
		bool newThread) {
	/* put address and size of the tls-region on the stack */
	uintptr_t tlsStart,tlsEnd;
	if(thread_getTLSRange(t,&tlsStart,&tlsEnd)) {
		frame->r[5] = tlsStart;
		frame->r[6] = tlsEnd - tlsStart;
	}
	else {
		/* no tls */
		frame->r[5] = 0;
		frame->r[6] = 0;
	}

	frame->r[4] = newThread ? tentryPoint : 0;
}
