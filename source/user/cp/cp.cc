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
#include <esc/cmdargs.h>
#include <esc/dir.h>
#include <esc/mem.h>
#include <ipc/proto/vterm.h>
#include <stdio.h>
#include <stdlib.h>
#include <env.h>

#define BUFFER_SIZE		(16 * 1024)

static ulong shmname;
static void *shm;
static bool rec = false;
static bool force = false;
static bool progress = false;
static ulong cols = 0;

static void usage(const char *name) {
	fprintf(stderr,"Usage: %s [-rfp] <source>... <dest>\n",name);
	fprintf(stderr,"\t-r    copy directories recursively\n");
	fprintf(stderr,"\t-f    overwrite existing files\n");
	fprintf(stderr,"\t-p    show a progress bar while copying\n");
	exit(EXIT_FAILURE);
}

static void createBuffer(void) {
	/* create shm file */
	int fd = pshm_create(IO_READ | IO_WRITE,0666,&shmname);
	if(fd < 0)
		error("pshm_create");
	/* mmap it */
	shm = mmap(NULL,BUFFER_SIZE,0,PROT_READ | PROT_WRITE,MAP_SHARED,fd,0);
	if(!shm)
		error("mmap");
}

static void printSize(size_t size) {
	if(size >= 1024 * 1024 * 1024)
		printf("%4zu GiB",size / (1024 * 1024 * 1024));
	else if(size >= 1024 * 1024)
		printf("%4zu MiB",size / (1024 * 1024));
	else if(size >= 1024)
		printf("%4zu KiB",size / 1024);
	else
		printf("%4zu B  ",size);
}

static void copyFile(const char *src,const char *dest) {
	sFileInfo info;
	if(stat(dest,&info) == 0 && !force) {
		printe("skipping '%s', '%s' exists",src,dest);
		return;
	}

	int infd = open(src,IO_READ);
	if(infd < 0) {
		printe("open of '%s' for reading failed",src);
		return;
	}

	int outfd = open(dest,IO_WRITE | IO_CREATE | IO_TRUNCATE);
	if(outfd < 0) {
		close(infd);
		printe("open of '%s' for writing failed",dest);
		return;
	}

	/* we don't care whether that is supported or not */
	if(sharefile(infd,shm) < 0) {}
	if(sharefile(outfd,shm) < 0) {}

	ssize_t res;
	if(progress) {
		size_t total = filesize(infd);
		size_t pos = 0;
		size_t lastPos = -1;
		ulong lastSteps = -1;
		ulong totalSteps = cols - (25 + SSTRLEN(": 0000 KiB of 0000 KiB []"));
		while((res = read(infd,shm,BUFFER_SIZE)) > 0) {
			if(write(outfd,shm,res) != res) {
				printe("write to '%s' failed",dest);
				break;
			}

			pos += res;
			ulong steps = ((ullong)totalSteps * pos) / total;
			if(steps != lastSteps || pos - lastPos > 1024 * 1024) {
				printf("\r%-25.25s: ",src);
				printSize(pos);
				printf(" of ");
				printSize(total);
				printf(" [");
				for(ulong i = 0; i < steps; ++i)
					putchar('#');
				for(ulong i = 0; i < totalSteps - steps; ++i)
					putchar(' ');
				putchar(']');
				fflush(stdout);
				lastSteps = steps;
				lastPos = pos;
			}
		}
		if(res == 0)
			putchar('\n');
	}
	else {
		while((res = read(infd,shm,BUFFER_SIZE)) > 0) {
			if(write(outfd,shm,res) != res) {
				printe("write to '%s' failed",dest);
				break;
			}
		}
	}
	if(res < 0)
		printe("read from '%s' failed",src);

	close(outfd);
	close(infd);
}

static void copy(const char *src,const char *dest) {
	char srccpy[MAX_PATH_LEN];
	char dstcpy[MAX_PATH_LEN];
	strnzcpy(srccpy,src,sizeof(srccpy));
	char *filename = basename(srccpy);
	snprintf(dstcpy,sizeof(dstcpy),"%s/%s",dest,filename);

	if(isdir(src)) {
		if(!rec) {
			print("omitting directory '%s'",src);
			return;
		}

		if(mkdir(dstcpy) < 0) {
			printe("mkdir '%s' failed",dstcpy);
			return;
		}

		DIR *dir = opendir(src);
		sDirEntry e;
		while(readdir(dir,&e)) {
			if(strcmp(e.name,".") == 0 || strcmp(e.name,"..") == 0)
				continue;

			snprintf(srccpy,sizeof(srccpy),"%s/%s",src,e.name);
			copy(srccpy,dstcpy);
		}
		closedir(dir);
	}
	else
		copyFile(src,dstcpy);
}

int main(int argc,const char *argv[]) {
	int res = ca_parse(argc,argv,0,"r f p",&rec,&force,&progress);
	if(res < 0 || ca_getFreeCount() < 2) {
		printe("Invalid arguments: %s",ca_error(res));
		usage(argv[0]);
	}
	if(ca_hasHelp())
		usage(argv[0]);

	if(progress) {
		ipc::VTerm vterm(std::env::get("TERM").c_str());
		cols = vterm.getMode().cols;
	}

	createBuffer();

	size_t count = ca_getFreeCount();
	const char **files = ca_getFree();
	const char *dest = files[count - 1];
	if(!isdir(dest)) {
		if(count > 2)
			error("'%s' is not a directory, but there are multiple source-files.",dest);
		if(isdir(files[0]))
			error("'%s' is not a directory, but the source '%s' is.",dest,files[0]);
		copyFile(files[0],dest);
	}
	else {
		for(size_t i = 0; i < count - 1; ++i)
			copy(files[i],dest);
	}
	return EXIT_SUCCESS;
}
