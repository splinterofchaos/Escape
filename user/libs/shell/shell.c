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
#include <esc/env.h>
#include <esc/dir.h>
#include <esc/fileio.h>
#include <esc/heap.h>
#include <esc/proc.h>
#include <esc/signals.h>
#include <esc/io.h>
#include <esc/keycodes.h>
#include <fsinterface.h>
#include <string.h>
#include <stdlib.h>
#include <esccodes.h>
#include <ctype.h>
#include <errors.h>

#include "shell.h"
#include "history.h"
#include "completion.h"
#include "parser.h"
#include "exec/running.h"

static void shell_sigIntrpt(tSig sig,u32 data);
static u16 shell_toNextWord(char *buffer,u32 icharcount,u32 *icursorPos);
static u16 shell_toPrevWord(char *buffer,u32 *icursorPos);
extern int yyparse(void);

static bool resetReadLine = false;
static u32 tabCount = 0;
tFile *curStream = NULL;
char *curLine = NULL;
bool curIsStream = false;

void shell_init(void) {
	run_init();
	if(setSigHandler(SIG_INTRPT,shell_sigIntrpt) < 0)
		error("Unable to announce sig-handler for %d",SIG_INTRPT);
}

bool shell_prompt(void) {
	/* ensure that we start a new readline */
	char *path = (char*)malloc((MAX_PATH_LEN + 1) * sizeof(char));
	resetReadLine = true;
	if(path == NULL) {
		printf("ERROR: unable to get CWD\n");
		return false;
	}
	if(!getEnv(path,MAX_PATH_LEN + 1,"CWD")) {
		free(path);
		printf("ERROR: unable to get CWD\n");
		return false;
	}
	printf("\033[co;8]%s\033[co] # ",path);
	free(path);
	return true;
}

static void shell_sigIntrpt(tSig sig,u32 data) {
	/* TODO this is dangerous! we can't use the heap in signal-handlers */
	UNUSED(sig);
	UNUSED(data);
	printf("\n");
	/*
	if(cmds && cmdCount) {
		u32 i;
		for(i = 0; i < cmdCount; i++) {
			if(cmds[i].pid != 0) {
				sendSignalTo(cmds[i].pid,SIG_INTRPT,0);
				cmds[i].pid = 0;
			}
			if(cmds[i].pipe[0] != -1) {
				close(cmds[i].pipe[0]);
				cmds[i].pipe[0] = -1;
			}
			if(cmds[i].pipe[1] != -1) {
				close(cmds[i].pipe[1]);
				cmds[i].pipe[1] = -1;
			}
		}
	}
	else
		*/shell_prompt();
}

s32 shell_executeCmd(char *line,bool isFile) {
	s32 res;
	curIsStream = isFile;
	if(isFile) {
		char absp[MAX_PATH_LEN];
		abspath(absp,MAX_PATH_LEN,line);
		curStream = fopen(absp,"r");
		if(curStream == NULL)
			return errno;
	}
	curLine = line;
	resetPos();
	res = yyparse();
	run_gc();
	if(isFile)
		fclose(curStream);
	return res;
}

u32 shell_readLine(char *buffer,u32 max) {
	u32 cursorPos = 0;
	u32 i = 0;
	resetReadLine = false;

	/* disable "readline", enable "echo" */
	ioctl(STDOUT_FILENO,IOCTL_VT_DIS_RDLINE,NULL,0);
	ioctl(STDOUT_FILENO,IOCTL_VT_EN_ECHO,NULL,0);

	/* ensure that the line is empty */
	*buffer = '\0';
	while(i < max) {
		char c;
		s32 cmd,n1,n2,n3;
		c = scanc();
		if(c != '\033')
			continue;
		cmd = freadesc(stdin,&n1,&n2,&n3);
		/* skip other escape-codes */
		if(cmd != ESCC_KEYCODE)
			continue;

		/* maybe we've received a ^C. if so do a reset */
		if(resetReadLine) {
			i = 0;
			cursorPos = 0;
			resetReadLine = false;
		}

		if(n3 != 0 || (n1 != '\t' && n1 != '\n' && !isprint(n1))) {
			if(shell_handleSpecialKey(buffer,n2,n3,&cursorPos,&i) || !isprint(n1))
				continue;
		}
		if(n1 == '\t') {
			shell_complete(buffer,&cursorPos,&i);
			continue;
		}
		/* if another key has been pressed, reset tab-counting */
		tabCount = 0;

		/* echo */
		printc(n1);
		flush();

		/* put the newline at the end */
		if(n1 == '\n') {
			buffer[i++] = n1;
			break;
		}
		/* not at the end */
		if(cursorPos < i) {
			u32 x;
			/* at first move all one char forward */
			memmove(buffer + cursorPos + 1,buffer + cursorPos,i - cursorPos);
			buffer[cursorPos] = n1;
			/* now write the chars to vterm */
			for(x = cursorPos + 1; x <= i; x++)
				printc(buffer[x]);
			/* and walk backwards */
			printf("\033[ml;%d]",i - cursorPos);
		}
		/* we are at the end of the input */
		else {
			/* put in buffer */
			buffer[cursorPos] = n1;
		}

		cursorPos++;
		i++;
	}

	/* enable "readline" */
	ioctl(STDOUT_FILENO,IOCTL_VT_EN_RDLINE,NULL,0);

	buffer[i] = '\0';
	return i;
}

bool shell_handleSpecialKey(char *buffer,s32 keycode,s32 modifier,u32 *cursorPos,u32 *charcount) {
	bool res = false;
	char *line = NULL;
	u32 icursorPos = *cursorPos;
	u32 icharcount = *charcount;
	switch(keycode) {
		case VK_W:
			if(modifier & STATE_CTRL) {
				u32 oldPos = icursorPos;
				u16 count = shell_toPrevWord(buffer,&icursorPos);
				if(count > 0) {
					memmove(buffer + icursorPos,buffer + oldPos,icharcount - oldPos);
					icharcount -= count;
					buffer[icharcount] = '\0';
					/* send backspaces */
					while(count-- > 0)
						printc('\b');
					flush();
				}
				res = true;
			}
			break;

		case VK_BACKSP:
			if(icursorPos > 0) {
				/* remove last char */
				if(icursorPos < icharcount)
					memmove(buffer + icursorPos - 1,buffer + icursorPos,icharcount - icursorPos);
				icharcount--;
				icursorPos--;
				buffer[icharcount] = '\0';
				/* send backspace */
				printc('\b');
				flush();
			}
			res = true;
			break;
		/* write escape-code back */
		case VK_DELETE:
			/* delete is the same as moving cursor one step right and pressing backspace */
			if(icursorPos < icharcount) {
				printf("\033[mr]");
				icursorPos++;
				shell_handleSpecialKey(buffer,VK_BACKSP,0,&icursorPos,&icharcount);
			}
			res = true;
			break;
		case VK_HOME:
			if(icursorPos > 0) {
				printf("\033[ml;%d]",icursorPos);
				icursorPos = 0;
			}
			res = true;
			break;
		case VK_END:
			if(icursorPos < icharcount) {
				printf("\033[mr;%d]",icharcount - icursorPos);
				icursorPos = icharcount;
			}
			res = true;
			break;
		case VK_RIGHT:
			if(icursorPos < icharcount) {
				/* to next word */
				if(modifier & STATE_CTRL) {
					u16 count = shell_toNextWord(buffer,icharcount,&icursorPos);
					if(count > 0)
						printf("\033[mr;%d]",count);
				}
				else {
					icursorPos++;
					printf("\033[mr]");
				}
			}
			res = true;
			break;
		case VK_LEFT:
			if(icursorPos > 0) {
				/* to prev word */
				if(modifier & STATE_CTRL) {
					u16 count = shell_toPrevWord(buffer,&icursorPos);
					if(count > 0)
						printf("\033[ml;%d]",count);
				}
				else {
					icursorPos--;
					printf("\033[ml]");
				}
			}
			res = true;
			break;
		case VK_UP:
			line = shell_histUp();
			res = true;
			break;
		case VK_DOWN:
			line = shell_histDown();
			res = true;
			break;
	}

	if(line != NULL) {
		u32 pos = *charcount;
		u32 i,len = strlen(line);

		/* go to line end */
		printf("\033[mr;%d]",*charcount - *cursorPos);
		/* delete chars */
		while(pos-- > 0)
			printc('\b');

		/* replace in buffer and write string to vterm */
		memcpy(buffer,line,len + 1);
		for(i = 0; i < len; i++)
			printc(buffer[i]);

		/* now send the commands */
		flush();

		/* set the cursor to the end */
		*cursorPos = len;
		*charcount = len;
	}
	else {
		*cursorPos = icursorPos;
		*charcount = icharcount;
	}

	return res;
}

static u16 shell_toNextWord(char *buffer,u32 icharcount,u32 *icursorPos) {
	u16 count = 0;
	/* skip first whitespace */
	while(*icursorPos < icharcount && isspace(buffer[*icursorPos])) {
		count++;
		(*icursorPos)++;
	}
	/* walk to last whitespace */
	while(*icursorPos < icharcount && !isspace(buffer[*icursorPos])) {
		count++;
		(*icursorPos)++;
	}
	return count;
}

static u16 shell_toPrevWord(char *buffer,u32 *icursorPos) {
	u16 count = 0;
	/* skip first whitespace */
	while(*icursorPos > 0 && isspace(buffer[*icursorPos - 1])) {
		count++;
		(*icursorPos)--;
	}
	/* walk to last whitespace */
	while(*icursorPos > 0 && !isspace(buffer[*icursorPos - 1])) {
		count++;
		(*icursorPos)--;
	}
	return count;
}

void shell_complete(char *line,u32 *cursorPos,u32 *length) {
	u32 icursorPos = *cursorPos;
	u32 i,cmdlen,ilength = *length;
	char *orgLine = line;

	/* ignore tabs when the cursor is not at the end of the input */
	if(false && icursorPos == ilength) {
#if 0
		sShellCmd **matches;
		sShellCmd **cmd;
		sCmdToken *tokens;
		char *token;
		u32 tokLen,tokCount;
		bool searchPath;

		/* get tokens */
		line[ilength] = '\0';
		tokens = tok_get(line,&tokCount);
		if(tokens == NULL)
			return;

		/* get matches for last token */
		if(tokCount > 0)
			token = tokens[tokCount - 1].str;
		else
			token = (char*)"";
		tokLen = strlen(token);
		searchPath = tokCount <= 1 && (tokCount == 0 || strchr(tokens[0].str,'/') == NULL);
		matches = compl_get(token,tokLen,0,false,searchPath);
		if(matches == NULL || matches[0] == NULL) {
			/* beep because we have found no match */
			printc('\a');
			flush();
			tok_free(tokens,tokCount);
			compl_free(matches);
			return;
		}

		/* found one match? */
		if(matches[1] == NULL) {
			char last;
			tabCount = 0;

			/* add chars */
			cmdlen = strlen(matches[0]->name);
			if(matches[0]->complStart == -1)
				i = ilength;
			else
				i = matches[0]->complStart;
			for(; i < cmdlen; i++) {
				char c = matches[0]->name[i];
				orgLine[ilength++] = c;
				printc(c);
			}

			/* append '/' or ' ' depending on wether its a dir or not */
			last = MODE_IS_DIR(matches[0]->mode) ? '/' : ' ';
			if(orgLine[ilength - 1] != last) {
				orgLine[ilength++] = last;
				printc(last);
			}

			/* set length and cursor */
			*length = ilength;
			*cursorPos = ilength;
			flush();
		}
		else {
			char last;
			u32 oldLen = ilength;
			bool allEqual;
			/* note that we can assume here that its the same for all matches because it just makes
			 * a difference if we're completing in a subdir. If we are in a subdir all matches
			 * are in that subdir (otherwise they are no matches ;)). So complStart is the last '/'
			 * for all commands in this case */
			if(matches[0]->complStart == -1)
				i = ilength;
			else
				i = matches[0]->complStart;
			for(; i < MAX_CMDNAME_LEN; i++) {
				/* check if the char is equal in all commands */
				allEqual = true;
				cmd = matches;
				last = (*cmd)->name[i];
				cmd++;
				while(*cmd != NULL) {
					if((*cmd)->name[i] != last) {
						allEqual = false;
						break;
					}
					cmd++;
				}

				/* if so, add it */
				if(allEqual) {
					orgLine[ilength++] = last;
					printc(last);
				}
				else
					break;
			}

			/* have we added something? */
			if(oldLen != ilength) {
				*length = ilength;
				*cursorPos = ilength;
				/* beep */
				printc('\a');
				flush();
			}
			/* show all on second tab */
			else if(tabCount == 0) {
				tabCount++;
				/* beep */
				printc('\a');
				flush();
			}
			else {
				tabCount = 0;

				/* print all matching commands */
				printf("\n");
				cmd = matches;
				while(*cmd != NULL) {
					printf("%s%s",(*cmd)->name,MODE_IS_DIR((*cmd)->mode) ? "/ " : " ");
					cmd++;
				}

				/* print the prompt + entered chars again */
				printf("\n");
				shell_prompt();
				/* we don't want to reset the read here */
				resetReadLine = false;
				printf("%s",line);
			}
		}

		tok_free(tokens,tokCount);
		compl_free(matches);
#endif
	}
}
