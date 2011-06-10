/*
** 2011 June 09
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** Internal definitions for XJD1
*/
#ifndef _XJD1INT_H
#define _XJD1INT_H

#include "xjd1.h"
#include "parse.h"
#include "xjd1.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#define PRIVATE

typedef unsigned char u8;
typedef struct Command Command;
typedef struct DataSrc DataSrc;
typedef struct Expr Expr;
typedef struct ExprItem ExprItem;
typedef struct ExprList ExprList;
typedef struct Parse Parse;
typedef struct PoolChunk PoolChunk;
typedef struct Pool Pool;
typedef struct Query Query;
typedef struct String String;
typedef struct Token Token;

/* A single allocation from the Pool allocator */
struct PoolChunk {
  PoolChunk *pNext;                 /* Next chunk on list of them all */
};

/* A memory allocation pool */
struct Pool {
  PoolChunk *pChunk;                /* List of all memory allocations */
  char *pSpace;                     /* Space available for allocation */
  int nSpace;                       /* Bytes available in pSpace */
};

/* Execution context */
struct xjd1_context {
  int nRef;                         /* Reference count */
  u8 isDying;                       /* True if has been deleted */
  int (*xLog)(const char*,void*);   /* Error logging function */
  void *pLogArg;                    /* 2nd argument to xLog() */
};

/* An open database connection */
struct xjd1 {
  xjd1_context *pContext;           /* Execution context */
  int nRef;                         /* Reference count */
  u8 isDying;                       /* True if has been closed */
  xjd1_stmt *pStmt;                 /* list of all prepared statements */
  
};

/* A prepared statement */
struct xjd1_stmt {
  xjd1 *pConn;                      /* Database connection */
  xjd1_stmt *pNext, *pPrev;         /* List of all statements */
  Pool sPool;                       /* Memory pool used for parsing */
  int nRef;                         /* Reference count */
  u8 isDying;                       /* True if has been closed */
  char *zCode;                      /* Text of the query */
  Command *pCmd;                    /* Parsed command */
  char *zErrMsg;                    /* Error message */
};

/* A variable length string */
struct String {
  Pool *pPool;                      /* Memory allocation pool or NULL */
  char *zBuf;                       /* String content */
  int nUsed;                        /* Slots used.  Not counting final 0 */
  int nAlloc;                       /* Space allocated */
};

/* A token into to the parser */
struct Token {
  const char *z;                    /* Text of the token */
  int n;                            /* Number of characters */
};

/* A single element of an expression list */
struct ExprItem {
  Token tkAs;
  Expr *pExpr;
};

/* A list of expressions */
struct ExprList {
  int nEItem;
  ExprItem **apEItem;
};

/* A node of an expression */
struct Expr {
  u8 eType;
  union {
    struct {
      Expr *pLeft;
      Expr *pRight;
    } binop;
    struct {
      Expr *p;
    } uniop;
    Token tk;
    struct {
      Token funcname;
      ExprList funcarg;
    } func;
    Query *q;
  } u;
};

/* Parsing context */
struct Parse {
  Pool *pPool;                    /* Memory allocation pool */
  Command *pCmd;                  /* Results */
  Token sTok;                     /* Last token seen */
  int errCode;                    /* Error code */
  String errMsg;                  /* Error message string */
};

/* A query statement */
struct Query {
  u8 eQType;
  union {
    struct {
      Query *pLeft;
      Query *pRight;
    } compound;
    struct {
      ExprList *pCol;
      DataSrc *pFrom;
      Expr *pWhere;
      ExprList *pGroupBy;
      ExprList *pHaving;
      ExprList *pLimit;
    } simple;
  } u;
};

/* Any command, including but not limited to a query */
struct Command {
  int eCmdType;
  union {
    struct {
      Token id;
    } trans;
    struct {
      int ifExists;
      Token name;
    } crtab;
  } u;
};

struct DataSrc {
  int eDSType;
  Token asId;
  union {
    struct {
      DataSrc *pLeft;
      DataSrc *pRight;
    } join;
    struct {
      Token name;
    } tab;
    struct {
      DataSrc *pNext;
      Token opName;
      ExprList *pList;
    } flatten;
    struct {
      Query *q;
    } subq;
  } u;
};


/******************************** context.c **********************************/
void xjd1ContextUnref(xjd1_context*);

/******************************** conn.c *************************************/
void xjd1Unref(xjd1*);

/******************************** malloc.c ***********************************/
Pool *xjd1PoolNew(void);
void xjd1PoolClear(Pool*);
void xjd1PoolDelete(Pool*);
void *xjd1PoolMalloc(Pool*, int);
char *xjd1PoolDup(Pool*, const char *, int);

/******************************** string.c ***********************************/
void xjd1StringInit(String*, Pool*, int);
String *xjd1StringNew(Pool*, int);
int xjd1StringAppend(String*, const char*, int);
#define xjd1StringText(S)      ((S)->zBuf)
#define xjd1StringLen(S)       ((S)->nUsed)
#define xjd1StringTruncate(S)  ((S)->nUsed=0)
void xjd1StringClear(String*);
void xjd1StringDelete(String*);
int xjd1StringVAppendf(String*, const char*, va_list);
int xjd1StringAppendf(String, const char*, ...);


/******************************** tokenize.c *********************************/
#define xjd1Isspace(x)   (xjd1CtypeMap[(unsigned char)(x)]&0x01)
#define xjd1Isalnum(x)   (xjd1CtypeMap[(unsigned char)(x)]&0x06)
#define xjd1Isalpha(x)   (xjd1CtypeMap[(unsigned char)(x)]&0x02)
#define xjd1Isdigit(x)   (xjd1CtypeMap[(unsigned char)(x)]&0x04)
#define xjd1Isxdigit(x)  (xjd1CtypeMap[(unsigned char)(x)]&0x08)
#define xjd1Isident(x)   (xjd1CtypeMap[(unsigned char)(x)]&0x46)



#endif /* _XJD1INT_H */
