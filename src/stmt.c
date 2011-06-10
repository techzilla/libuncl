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
  xjd1RunParser(p, zStmt, pN);
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
