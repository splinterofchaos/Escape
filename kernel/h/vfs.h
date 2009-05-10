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

#ifndef VFS_H_
#define VFS_H_

#include "common.h"
#include "proc.h"
#include <sllist.h>

/* special service-client names */
#define SERVICE_CLIENT_KERNEL		"k"
#define SERVICE_CLIENT_ALL			"a"

/* some additional types for the kernel */
#define MODE_TYPE_SERVUSE			00200000
#define MODE_TYPE_SERVICE			00400000
#define MODE_TYPE_PIPECON			01000000
#define MODE_TYPE_PIPE				02000000
#define MODE_SERVICE_SINGLEPIPE		04000000

/* GFT flags */
enum {
	/* no read and write */
	VFS_NOACCESS = 0,
	VFS_READ = 1,
	VFS_WRITE = 2
};

/* a node in our virtual file system */
typedef struct sVFSNode sVFSNode;
/* the function for read-requests on info-nodes */
typedef s32 (*fRead)(tPid pid,sVFSNode *node,u8 *buffer,u32 offset,u32 count);
/* callback function for the default read-handler */
typedef void (*fReadCallBack)(sVFSNode *node,void *buffer);

struct sVFSNode {
	char *name;
	/* number of open files for this node */
	u8 refCount;
	/* 0 means unused; stores permissions and the type of node */
	u32 mode;
	/* for the vfs-structure */
	sVFSNode *parent;
	sVFSNode *prev;
	sVFSNode *next;
	sVFSNode *firstChild;
	sVFSNode *lastChild;
	/* the handler that performs a read-operation on this node */
	fRead readHandler;
	/* the owner of this node: used for service-usages */
	tPid owner;
	union {
		/* for service-usages */
		struct {
			/* we have to lock reads */
			tFileNo locked;
			/* a list of clients for single-pipe-services */
			sSLList *singlePipeClients;
			/* a list for sending messages to the service */
			sSLList *sendList;
			/* a list for reading messages from the service */
			sSLList *recvList;
		} servuse;
		/* for all other nodes-types */
		struct {
			/* size of the buffer */
			u16 size;
			/* currently used size */
			u16 pos;
			void *cache;
		} def;
	} data;
};

/**
 * Initializes the virtual file system
 */
void vfs_init(void);

/**
 * Checks wether the process with given pid has the permission to do the given stuff with <nodeNo>.
 *
 * @param pid the process-id
 * @param nodeNo the node-number
 * @param flags specifies what you want to do (VFS_READ | VFS_WRITE)
 * @return 0 if the process has permission or the error-code
 */
s32 vfs_hasAccess(tPid pid,tVFSNodeNo nodeNo,u8 flags);

/**
 * Inherits the given file for the current process
 *
 * @param pid the process-id
 * @param file the file
 * @return file the file to use (may be the same)
 */
tFileNo vfs_inheritFileNo(tPid pid,tFileNo file);

/**
 * Increases the references of the given file
 *
 * @param file the file
 * @return 0 on success
 */
s32 vfs_incRefs(tFileNo file);

/**
 * Determines the node-number for the given file
 *
 * @param file the file
 * @return the node-number or an error (both may be negative :/, check with vfsn_isValidNodeNo())
 */
tVFSNodeNo vfs_getNodeNo(tFileNo file);

/**
 * Opens the file with given number and given flags. That means it walks through the global
 * file table and searches for a free entry or an entry for that file.
 * Note that multiple processes may read from the same file simultaneously but NOT write!
 *
 * @param pid the process-id with which the file should be opened
 * @param flags wether it is a virtual or real file and wether you want to read or write
 * @param nodeNo the node-number (in the virtual or real environment)
 * @return the file if successfull or < 0 (ERR_FILE_IN_USE, ERR_NO_FREE_FD)
 */
tFileNo vfs_openFile(tPid pid,u8 flags,tVFSNodeNo nodeNo);

/**
 * Opens a file for the kernel. Creates a node for it, if not already done
 *
 * @param pid the process-id with which the file should be opened
 * @param nodeNo the service-node-number
 * @return the file-number or a negative error-code
 */
tFileNo vfs_openFileForKernel(tPid pid,tVFSNodeNo nodeNo);

/**
 * Checks wether we are at EOF in the given file
 *
 * @param pid the process-id
 * @param file the file
 * @return true if at EOF
 */
bool vfs_eof(tPid pid,tFileNo file);

/**
 * Sets the position for the given file
 *
 * @param pid the process-id
 * @param file the file
 * @param position the new position
 * @return 0 on success
 */
s32 vfs_seek(tPid pid,tFileNo file,u32 position);

/**
 * Reads max. count bytes from the given file into the given buffer and returns the number
 * of read bytes.
 *
 * @param pid will be used to check wether the service writes or a service-user
 * @param file the file
 * @param buffer the buffer to write to
 * @param count the max. number of bytes to read
 * @return the number of bytes read
 */
s32 vfs_readFile(tPid pid,tFileNo file,u8 *buffer,u32 count);

/**
 * Writes count bytes from the given buffer into the given file and returns the number of written
 * bytes.
 *
 * @param pid will be used to check wether the service writes or a service-user
 * @param file the file
 * @param buffer the buffer to read from
 * @param count the number of bytes to write
 * @return the number of bytes written
 */
s32 vfs_writeFile(tPid pid,tFileNo file,u8 *buffer,u32 count);

/**
 * Closes the given file. That means it calls proc_closeFile() and decrements the reference-count
 * in the global file table. If there are no references anymore it releases the slot.
 *
 * @param file the file
 */
void vfs_closeFile(tFileNo file);

/**
 * Creates a service-node for the given process and given name
 *
 * @param pid the process-id
 * @param name the service-name
 * @param type single-pipe or multi-pipe
 * @return 0 if ok, negative if an error occurred
 */
s32 vfs_createService(tPid pid,const char *name,u32 type);

/**
 * Checks wether there is a message for the given process. That if the process is a service
 * and should serve a client or if the process has got a message from a service.
 *
 * @param pid the process-id
 * @param events the events to wait for
 * @return true if there is a message
 */
bool vfs_msgAvailableFor(tPid pid,u8 events);

/**
 * For services: Looks wether a client wants to be served and return the node-number
 *
 * @param pid the service-process-id
 * @param vfsNodes an array of VFS-nodes to check for clients
 * @param count the size of <vfsNodes>
 * @return the error-code or the node-number of the client
 */
s32 vfs_getClient(tPid pid,tVFSNodeNo *vfsNodes,u32 count);

/**
 * Opens a file for the client with given process-id. This works just for multipipe-services!
 *
 * @param pid the own process-id
 * @param nodeNo the service-node-number
 * @param clientId the process-id of the desired client
 * @return the file or a negative error-code
 */
tFileNo vfs_openClientProc(tPid pid,tVFSNodeNo nodeNo,tPid clientId);

/**
 * Opens a file for a client of the given service-node
 *
 * @param pid the process to use
 * @param vfsNodes an array of VFS-nodes to check for clients
 * @param count the size of <vfsNodes>
 * @param node will be set to the node-number from which the client has been taken
 * @return the error-code (negative) or the file to use
 */
tFileNo vfs_openClient(tPid pid,tVFSNodeNo *vfsNodes,u32 count,tVFSNodeNo *servNode);

/**
 * Removes the service with given node-number
 *
 * @param pid the process-id
 * @param nodeNo the node-number of the service
 */
s32 vfs_removeService(tPid pid,tVFSNodeNo nodeNo);

/**
 * Creates a process-node with given pid and handler-function
 *
 * @param pid the process-id
 * @param handler the read-handler
 * @return true if successfull
 */
bool vfs_createProcess(tPid pid,fRead handler);

/**
 * Removes the node for the process with given id from the VFS
 *
 * @param pid the process-id
 */
void vfs_removeProcess(tPid pid);

/**
 * The default-read-handler
 */
s32 vfs_defReadHandler(tPid pid,sVFSNode *node,u8 *buffer,u32 offset,u32 count);

/**
 * Creates space, calls the callback which should fill the space
 * with data and writes the corresponding part to the buffer of the user
 *
 * @param pid the process-id
 * @param node the vfs-node
 * @param buffer the buffer
 * @param offset the offset
 * @param count the number of bytes to copy
 * @param dataSize the total size of the data
 * @param callback the callback-function
 */
s32 vfs_readHelper(tPid pid,sVFSNode *node,u8 *buffer,u32 offset,u32 count,u32 dataSize,
		fReadCallBack callback);

/**
 * The read-handler for service-usages
 *
 * @param pid the process-id
 * @param node the VFS node
 * @param buffer the buffer where to copy the info to
 * @param offset the offset where to start
 * @param count the number of bytes
 * @return the number of read bytes
 */
s32 vfs_serviceUseReadHandler(tPid pid,sVFSNode *node,u8 *buffer,u32 offset,u32 count);

#if DEBUGGING

/**
 * Prints all used entries in the global file table
 */
void vfs_dbg_printGFT(void);

/**
 * @return the number of entries in the global file table
 */
u32 vfs_dbg_getGFTEntryCount(void);

#endif

#endif /* VFS_H_ */
