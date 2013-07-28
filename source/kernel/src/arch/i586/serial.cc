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
#include <sys/arch/i586/serial.h>
#include <sys/arch/i586/ports.h>
#include <sys/spinlock.h>
#include <assert.h>

enum {
	DLR_LO	= 0,
	DLR_HI	= 1,
	IER		= 1,	/* interrupt enable register */
	FCR		= 2,	/* FIFO control register */
	LCR		= 3,	/* line control register */
	MCR		= 4,	/* modem control register */
};

static int ser_isTransmitEmpty(uint16_t port);
static void ser_initPort(uint16_t port);

static const uint16_t ports[] = {
	/* COM1 */	0x3F8,
	/* COM2 */	0x2E8,
	/* COM3 */	0x2F8,
	/* COM4 */	0x3E8
};
static klock_t serialLock;

void ser_init(void) {
	ser_initPort(ports[SER_COM1]);
}

void ser_out(uint16_t port,uint8_t byte) {
	uint16_t ioport;
	assert(port < ARRAY_SIZE(ports));
	ioport = ports[port];
	spinlock_aquire(&serialLock);
	while(ser_isTransmitEmpty(ioport) == 0);
	Ports::out<uint8_t>(ioport,byte);
	spinlock_release(&serialLock);
}

static int ser_isTransmitEmpty(uint16_t port) {
	return Ports::in<uint8_t>(port + 5) & 0x20;
}

static void ser_initPort(uint16_t port) {
	Ports::out<uint8_t>(port + LCR,0x80);		/* Enable DLAB (set baud rate divisor) */
	Ports::out<uint8_t>(port + DLR_LO,0x01);	/* Set divisor to 1 (lo byte) 115200 baud */
	Ports::out<uint8_t>(port + DLR_HI,0x00);	/*                  (hi byte) */
	Ports::out<uint8_t>(port + LCR,0x03);	/* 8 bits, no parity, one stop bit */
	Ports::out<uint8_t>(port + IER,0);		/* disable interrupts */
	Ports::out<uint8_t>(port + FCR,7);
	Ports::out<uint8_t>(port + MCR,3);
}