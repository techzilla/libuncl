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
#include "sqlite3.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#define PRIVATE

typedef unsigned char u8;

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
  int nRef;                         /* Reference count */
  u8 isDying;                       /* True if has been closed */
};



/******************************** context.c **********************************/
void xjd1ContextUnref(xjd1_context*);

/******************************** conn.c *************************************/
void xjd1Unref(xjd1*);


#endif /* _XJD1INT_H */
