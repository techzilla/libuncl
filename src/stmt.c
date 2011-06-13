/*
** Copyright (c) 2011 D. Richard Hipp
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the Simplified BSD License (also
** known as the "2-Clause License" or "FreeBSD License".)
**
** This program is distributed in the hope that it will be useful,
** but without any warranty; without even the implied warranty of
** merchantability or fitness for a particular purpose.
**
** Author contact information:
**   drh@hwaci.com
**   http://www.hwaci.com/drh/
**
*************************************************************************
** Interfaces to prepared statment objects.
*/
#include "xjd1Int.h"

/*
** Create a new prepared statement for database connection pConn.  The
** program code to be parsed is zStmt.  Return the new statement in *ppNew.
** Write the number of zStmt bytes read into *pN.
*/
int xjd1_stmt_new(xjd1 *pConn, const char *zStmt, xjd1_stmt **ppNew, int *pN){
  xjd1_stmt *p;
  int dummy;

  *pN = strlen(zStmt);
  *ppNew = p = malloc( sizeof(*p) );
  if( p==0 ) return XJD1_NOMEM;
  memset(p, 0, sizeof(*p));
  p->pConn = pConn;
  p->pNext = pConn->pStmt;
  pConn->pStmt = p;
  if( pN==0 ) pN = &dummy;
  xjd1RunParser(pConn, p, zStmt, pN);
  return XJD1_OK;
}

/*
** Configure a prepared statement.
*/
int xdj1_stmt_config(xjd1_stmt *pStmt, int op, ...){
  return XJD1_UNKNOWN;
}

/*
** Delete a prepared statement.
*/
int xjd1_stmt_delete(xjd1_stmt *pStmt){
  if( pStmt==0 ) return XJD1_OK;
  pStmt->isDying = 1;
  if( pStmt->nRef>0 ) return XJD1_OK;
  if( pStmt->pPrev ){
    pStmt->pPrev->pNext = pStmt->pNext;
  }else{
    assert( pStmt->pConn->pStmt==pStmt );
    pStmt->pConn->pStmt = pStmt->pNext;
  }
  if( pStmt->pNext ){
    pStmt->pNext->pPrev = pStmt->pPrev;
  }
  xjd1Unref(pStmt->pConn);
  xjd1PoolClear(&pStmt->sPool);
  free(pStmt);
  return XJD1_OK;
}

/*
** Execute a prepared statement up to its next return value or until
** it completes.
*/
int xjd1_stmt_step(xjd1_stmt *pStmt){
  return XJD1_DONE;
}

/*
** Rewind a prepared statement back to the beginning.
*/
int xjd1_stmt_rewind(xjd1_stmt *pStmt){
  return XJD1_OK;
}

/*
** Return the output value of a prepared statement resulting from
** its most recent xjd1_stmt_step() call.
*/
int xjd1_stmt_value(xjd1_stmt *pStmt, const char **pzValue){
  *pzValue = 0;
  return XJD1_MISUSE;
}

/*
** Construct a human-readable listing of a prepared statement showing
** its internal structure.  Used for debugging and analysis only.
**
** The return string is obtained from malloc and must be freed by
** the caller.
*/
char *xjd1_stmt_debug_listing(xjd1_stmt *p){
  String x;
  xjd1StringInit(&x, 0, 0);
  xjd1TraceCommand(&x, 0, p->pCmd);
  return x.zBuf;
}
