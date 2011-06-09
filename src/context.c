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
** Execution context
*/
#include "xjd1Int.h"


int xjd1_context_new(xjd1_context **ppNew){
  xjd1_context *p;

  *ppNew = p = malloc( sizeof(*p) );
  if( p ){
    memset(p, 0, sizeof(*p));
    return XJD1_OK;
  }else{
    return XJD1_NOMEM;
  }
}
int xjd1_context_config(xjd1_context *p, int op, ...){
  return XJD1_UNKNOWN;
}
int xjd1_context_delete(xjd1_context *p){
  if( p==0 ) return XJD1_OK;
  p->isDying = 1;
  if( p->nRef>0 ) return XJD1_OK;
  free(p);
  return XJD1_OK;
}

PRIVATE void xjd1ContextUnref(xjd1_context *p){
  if( p==0 ) return;
  p->nRef--;
  if( p->nRef<=0 && p->isDying ) xjd1_context_delete(p);
}
