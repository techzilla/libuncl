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
** Main interfaces
*/
#include "xjd1Int.h"

int xjd1_open(xjd1_context *pContext, const char *zURI, xjd1 **ppNewConn){
  xjd1 *pConn;

  *ppNewConn = pConn = malloc( sizeof(*pConn) );
  if( pConn==0 ) return XJD1_NOMEM;
  memset(pConn, 0, sizeof(*pConn));
  pConn->pContext = pContext;
  return XJD1_OK;
}
int xjd1_config(xjd1 *pConn, int op, ...){
  return XJD1_UNKNOWN;
}
int xjd1_close(xjd1 *pConn){
  if( pConn==0 ) return XJD1_OK;
  pConn->isDying = 1;
  if( pConn->nRef>0 ) return XJD1_OK;
  xjd1ContextUnref(pConn->pContext);
  free(pConn);
  return XJD1_OK;
}

PRIVATE void xjd1Unref(xjd1 *pConn){
  pConn->nRef--;
  if( pConn->nRef<=0 && pConn->isDying ) xjd1_close(pConn);
}
