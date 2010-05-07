/**
 * $Id: debug.h 350 2009-09-25 20:34:35Z nasmussen $
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

#ifndef DEBUG_H_
#define DEBUG_H_

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

extern u64 cpu_rdtsc(void);

/**
 * @return the cpu-cycles of the current thread
 */
u64 getCycles(void);

/**
 * Starts the timer, i.e. reads the current cpu-cycle count for this thread
 */
void dbg_startUTimer(void);

/**
 * Stops the timer, i.e. reads the current cpu-cycle count for this thread, calculates
 * the difference and prints "<prefix>: <cycles>\n".
 *
 * @param prefix the prefix to print
 */
void dbg_stopUTimer(char *prefix);

/**
 * Starts the timer (cpu_rdtsc())
 */
void dbg_startTimer(void);

/**
 * Stops the timer and prints "<prefix>: <clockCycles>\n"
 *
 * @param prefix the prefix for the output
 */
void dbg_stopTimer(char *prefix);

/**
 * Just intended for debugging. May be used for anything :)
 * It's just a system-call thats used by nothing else, so we can use it e.g. for printing
 * debugging infos in the kernel to points of time controlled by user-apps.
 */
void debug(void);

/**
 * Prints <byteCount> bytes at <addr>
 *
 * @param addr the start-address
 * @param byteCount the number of bytes
 */
void dumpBytes(void *addr,u32 byteCount);

/**
 * Prints <dwordCount> dwords at <addr>
 *
 * @param addr the start-address
 * @param dwordCount the number of dwords
 */
void dumpDwords(void *addr,u32 dwordCount);

/**
 * Prints <num> elements each <elsize> big of <array>
 *
 * @param array the array-address
 * @param num the number of elements
 * @param elsize the size of each element
 */
void dumpArray(void *array,u32 num,u32 elsize);

/**
 * Prints <byteCount> bytes at <addr> with debugf
 *
 * @param addr the start-address
 * @param byteCount the number of bytes
 */
void debugBytes(void *addr,u32 byteCount);

/**
 * Prints <dwordCount> dwords at <addr> with debugf
 *
 * @param addr the start-address
 * @param dwordCount the number of dwords
 */
void debugDwords(void *addr,u32 dwordCount);

/**
 * Prints the given char
 *
 * @param c the character
 */
extern void debugChar(char c);

/**
 * Same as debugf, but with the va_list as argument
 *
 * @param fmt the format
 * @param ap the argument-list
 */
void vdebugf(const char *fmt,va_list ap);

/**
 * Debugging version of printf. Supports the following formatting:
 * %d: signed integer
 * %u: unsigned integer, base 10
 * %o: unsigned integer, base 8
 * %x: unsigned integer, base 16 (small letters)
 * %X: unsigned integer, base 16 (big letters)
 * %b: unsigned integer, base 2
 * %s: string
 * %c: character
 *
 * @param fmt the format
 */
void debugf(const char *fmt, ...);

/**
 * Prints the given unsigned integer in the given base
 *
 * @param n the number
 * @param base the base
 */
void debugUint(u32 n,u8 base);

/**
 * Prints the given integer in base 10
 *
 * @param n the number
 */
void debugInt(s32 n);

/**
 * Prints the given string
 *
 * @param s the string
 */
void debugString(char *s);

#ifdef __cplusplus
}
#endif

#endif /* DEBUG_H_ */