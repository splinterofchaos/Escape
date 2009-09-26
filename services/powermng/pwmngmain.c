/**
 * $Id: randmain.c 328 2009-09-16 13:36:41Z nasmussen $
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
#include <esc/debug.h>
#include <esc/dir.h>
#include <esc/service.h>
#include <esc/io.h>
#include <esc/fileio.h>
#include <esc/ports.h>
#include <esc/signals.h>
#include <stdlib.h>
#include <messages.h>
#include <errors.h>

#define IOPORT_KB_CTRL				0x64
#define KBC_CMD_RESET				0xFE

#define ARRAY_INC_SIZE				16
#define PROC_BUFFER_SIZE			512
#define WAIT_TIMEOUT				1000

static void killProcs(void);
static int pidCompare(const void *p1,const void *p2);
static void waitForProc(tPid pid);
static void getProcName(tPid pid,char *name);

static sMsg msg;

int main(void) {
	tServ id,client;
	tMsgId mid;
	bool run = true;

	if(requestIOPorts(IOPORT_KB_CTRL,2) < 0)
		error("Unable to request io-port %d",IOPORT_KB_CTRL);

	id = regService("powermng",SERV_DEFAULT);
	if(id < 0)
		error("Unable to register service 'powermng'");

	/* wait for commands */
	while(run) {
		tFD fd = getClient(&id,1,&client);
		if(fd < 0)
			wait(EV_CLIENT);
		else {
			while(run && receive(fd,&mid,&msg,sizeof(msg)) > 0) {
				switch(mid) {
					case MSG_POWER_REBOOT:
						killProcs();
						debugf("Using pulse-reset line of 8042 controller to reset...\n");
						run = false;
						outByte(IOPORT_KB_CTRL,KBC_CMD_RESET);
						break;
					case MSG_POWER_SHUTDOWN:
						killProcs();
						debugf("System is stopped\n");
						debugf("You can turn off now\n");
						/* TODO we should use ACPI later here */
						break;
				}
			}
			close(fd);
		}
	}

	/* clean up */
	releaseIOPorts(IOPORT_KB_CTRL,2);
	unregService(id);
	return EXIT_SUCCESS;
}

static void killProcs(void) {
	char name[MAX_PROC_NAME_LEN + 1];
	sDirEntry e;
	tFD fd;
	tPid pid,own = getpid();
	u32 i,pidSize = ARRAY_INC_SIZE;
	u32 pidPos = 0;

	tPid *pids = (tPid*)malloc(sizeof(tPid) * pidSize);
	if(pids == NULL)
		error("Unable to alloc mem for pids");

	fd = opendir("/system/processes");
	if(fd < 0) {
		free(pids);
		error("Unable to open '/system/processes'");
	}

	while(readdir(&e,fd)) {
		if(strcmp(e.name,".") == 0 || strcmp(e.name,"..") == 0)
			continue;
		pid = atoi(e.name);
		if(pid == 0 || pid == own)
			continue;
		if(pidPos >= pidSize) {
			pidSize += ARRAY_INC_SIZE;
			pids = (tPid*)realloc(pids,sizeof(tPid) * pidSize);
			if(pids == NULL)
				error("Unable to alloc mem for pids");
		}
		pids[pidPos++] = pid;
	}
	closedir(fd);

	qsort(pids,pidPos,sizeof(tPid),pidCompare);
	for(i = 0; i < pidPos; i++) {
		getProcName(pids[i],name);
		debugf("Terminating process %d (%s)",pids[i],name);
		sendSignalTo(pids[i],SIG_TERM,0);
		waitForProc(pids[i]);
	}

	free(pids);
}

static int pidCompare(const void *p1,const void *p2) {
	/* descending */
	return *(tPid*)p2 - *(tPid*)p1;
}

static void waitForProc(tPid pid) {
	tFD fd;
	u32 time = 0;
	char path[SSTRLEN("/system/processes/") + 12];
	sprintf(path,"/system/processes/%d",pid);
	while(1) {
		fd = open(path,IO_READ);
		if(fd < 0)
			break;
		close(fd);
		if(time >= WAIT_TIMEOUT) {
			debugf("\nProcess does still exist after %d ms; killing it",time,pid);
			sendSignalTo(pid,SIG_KILL,0);
			break;
		}
		debugf(".");
		sleep(20);
		time += 20;
	}
	debugf("\n");
}

static void getProcName(tPid pid,char *name) {
	tFD fd;
	char buffer[PROC_BUFFER_SIZE];
	char path[SSTRLEN("/system/processes//info") + 12];
	sprintf(path,"/system/processes/%d/info",pid);
	fd = open(path,IO_READ);
	if(fd < 0)
		error("Unable to open '%s'",path);
	if(read(fd,buffer,ARRAY_SIZE(buffer) - 1) < 0) {
		close(fd);
		error("Reading '%s' failed",path);
	}
	buffer[ARRAY_SIZE(buffer) - 1] = '\0';
	sscanf(
		buffer,
		"%*s%*hu" "%*s%*hu" "%*s%s",
		name
	);
	close(fd);
}