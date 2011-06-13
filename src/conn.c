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
  int rc = XJD1_UNKNOWN;
  va_list ap;
  va_start(ap, op); 
  switch( op ){
    case XJD1_CONFIG_PARSERTRACE: {
      pConn->parserTrace = va_arg(ap, int);
      rc = XJD1_OK;
      break;
    }
    default: {
      break;
    }
  }
  va_end(ap);
  return rc;
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
