%code top {
	#include <stdio.h>
	int yylex (void);
	void yyerror(char const *,...);
	int bla;
}

%code requires {
	#include "ast/node.h"
	#include "ast/assignexpr.h"
	#include "ast/binaryopexpr.h"
	#include "ast/cmpexpr.h"
	#include "ast/conststrexpr.h"
	#include "ast/ifstmt.h"
	#include "ast/intexpr.h"
	#include "ast/stmtlist.h"
	#include "ast/unaryopexpr.h"
	#include "ast/varexpr.h"
	#include "ast/command.h"
	#include "ast/cmdexprlist.h"
	#include "ast/subcmd.h"
	#include "ast/redirfd.h"
	#include "ast/redirfile.h"
	#include "ast/forstmt.h"
	#include "ast/exprstmt.h"
	#include "ast/dstrexpr.h"
	#include "ast/whilestmt.h"
	#include "exec/env.h"
	#include "mem.h"
}
%union {
	int intval;
	char *strval;
	sASTNode *node;
}

%token T_IF
%token T_THEN
%token T_ELSE
%token T_FI
%token T_FOR
%token T_DO
%token T_DONE
%token T_WHILE

%token <intval> T_NUMBER
%token <strval> T_STRING
%token <strval> T_STRING_SCONST
%token <strval> T_VAR

%token T_ERR2OUT
%token T_OUT2ERR
%token T_APPEND

%type <node> stmtlist stmtlistr stmt expr exprstmt cmdexpr cmdexprlist cmd subcmd 
%type <node> cmdredirfd cmdredirin cmdredirout strlist strcomp

%nonassoc '>' '<' T_LEQ T_GEQ T_EQ T_NEQ T_ASSIGN
%left T_ADD T_SUB
%left T_MUL T_DIV T_MOD
%left T_NEG
%right T_POW

%destructor { free($$); } <strval>
%destructor { ast_destroy($$); } <node>

%%

start:
			stmtlist {
				sEnv *e = env_create();
				ast_execute(e,$1);
				ast_destroy($1);
				env_destroy(e);
			}
;

stmtlist:
			/* empty */							{ $$ = ast_createStmtList(); }
			| stmtlistr							{ $$ = $1; }
			| stmtlistr ';'					{ $$ = $1; }
;

stmtlistr:
			stmt										{ $$ = ast_createStmtList(); ast_addStmt($$,$1); }
			| stmtlistr ';' stmt		{ $$ = ast_addStmt($1,$3); }
;

stmt:
			T_IF '(' expr ')' T_THEN stmtlist T_FI {
				$$ = ast_createIfStmt($3,$6,NULL);
			}
			| T_IF '(' expr ')' T_THEN stmtlist T_ELSE stmtlist T_FI {
				$$ = ast_createIfStmt($3,$6,$8);
			}
			| T_FOR '(' expr ';' expr ';' expr ')' T_DO stmtlist T_DONE {
				$$ = ast_createForStmt($3,$5,$7,$10);
			}
			| T_WHILE '(' expr ')' T_DO stmtlist T_DONE {
				$$ = ast_createWhileStmt($3,$6);
			}
			| exprstmt {
				$$ = ast_createExprStmt($1);
			}
			| cmd {
				$$ = $1;
			}
;

strlist:
			strlist strcomp			{ $$ = $1; ast_addDStrComp($1,$2); }
			| strcomp						{ $$ = ast_createDStrExpr(); ast_addDStrComp($$,$1); }
;

strcomp:
			T_STRING						{ $$ = ast_createConstStrExpr($1); }
			| '{' expr '}'			{ $$ = $2; }
;

expr:
			cmdexpr							{ $$ = $1; }
			| exprstmt					{ $$ = $1; }
			| expr T_ADD expr		{ $$ = ast_createBinOpExpr($1,'+',$3); }
			| expr T_SUB expr		{ $$ = ast_createBinOpExpr($1,'-',$3); }
			| expr T_MUL expr		{ $$ = ast_createBinOpExpr($1,'*',$3); }
			| expr T_DIV expr		{ $$ = ast_createBinOpExpr($1,'/',$3); }
			| expr T_MOD expr		{ $$ = ast_createBinOpExpr($1,'%',$3); }
			| expr T_POW expr		{ $$ = ast_createBinOpExpr($1,'^',$3); }
			| T_SUB expr %prec T_NEG { $$ = ast_createUnaryOpExpr($2,UN_OP_NEG); }
			| expr '<' expr			{ $$ = ast_createCmpExpr($1,CMP_OP_LT,$3); }
			| expr '>' expr			{ $$ = ast_createCmpExpr($1,CMP_OP_GT,$3); }
			| expr T_LEQ expr		{ $$ = ast_createCmpExpr($1,CMP_OP_LEQ,$3); }
			| expr T_GEQ expr		{ $$ = ast_createCmpExpr($1,CMP_OP_GEQ,$3); }
			| expr T_EQ expr		{ $$ = ast_createCmpExpr($1,CMP_OP_EQ,$3); }
			| expr T_NEQ expr		{ $$ = ast_createCmpExpr($1,CMP_OP_NEQ,$3); }
			| '(' expr ')'			{ $$ = $2; }
;

exprstmt:
			T_VAR T_ASSIGN expr	{ $$ = ast_createAssignExpr(ast_createVarExpr($1),$3); }
			| '`' subcmd '`'		{ $$ = $2; ast_setRetOutput($2,true); }
;

cmdexpr:
			T_NUMBER						{ $$ = ast_createIntExpr($1); }
			| T_STRING					{ $$ = ast_createConstStrExpr($1); }
			| T_STRING_SCONST		{ $$ = ast_createConstStrExpr($1); }
			| '"' strlist '"'		{ $$ = $2; }
			| T_VAR							{ $$ = ast_createVarExpr($1); }
			| '{' expr '}'			{ $$ = $2; }
;

cmdexprlist:
			cmdexpr							{ $$ = ast_createCmdExprList(); ast_addCmdExpr($$,$1); }
			| cmdexprlist cmdexpr { $$ = $1; ast_addCmdExpr($1,$2); }
;

cmd:
			subcmd							{ $$ = $1; }
			| subcmd '&'				{ $$ = $1; ast_setRunInBG($1,true); }
;

subcmd:
			cmdexprlist cmdredirfd cmdredirin cmdredirout {
				$$ = ast_createCommand();
				ast_addSubCmd($$,ast_createSubCmd($1,$2,$3,$4));
			}
			| subcmd '|' cmdexprlist cmdredirfd cmdredirin cmdredirout {
				$$ = $1;
				ast_addSubCmd($1,ast_createSubCmd($3,$4,$5,$6));
			}
;

cmdredirfd:
			T_ERR2OUT						{ $$ = ast_createRedirFd(REDIR_ERR2OUT); }
			| T_OUT2ERR					{ $$ = ast_createRedirFd(REDIR_OUT2ERR); }
			| /* empty */				{ $$ = ast_createRedirFd(REDIR_NO); }
;

cmdredirin:
			'<' cmdexpr					{ $$ = ast_createRedirFile($2,REDIR_INFILE); }
			| /* empty */				{ $$ = ast_createRedirFile(NULL,REDIR_OUTCREATE); }
;

cmdredirout:
			'>' cmdexpr					{ $$ = ast_createRedirFile($2,REDIR_OUTCREATE); }
			| T_APPEND cmdexpr	{ $$ = ast_createRedirFile($2,REDIR_OUTAPPEND); }
			| /* empty */				{ $$ = ast_createRedirFile(NULL,REDIR_OUTCREATE); }
;

%%