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
** This file contains code used to implement methods for the DataSrc object.
*/
#include "xjd1Int.h"

/*
** Called after statement parsing to initalize every DataSrc object.
*/
int xjd1DataSrcInit(DataSrc *p, Query *pQuery){
  p->pQuery = pQuery;
  switch( p->eDSType ){
    case TK_COMMA: {
      xjd1DataSrcInit(p->u.join.pLeft, pQuery);
      xjd1DataSrcInit(p->u.join.pRight, pQuery);
      break;
    }
    case TK_SELECT: {
      xjd1QueryInit(p->u.subq.q, pQuery->pStmt, pQuery);
      break;
    }
    case TK_ID: {
      char *zSql = sqlite3_mprintf("SELECT x FROM \"%w\"", p->u.tab.zName);
      sqlite3_prepare_v2(pQuery->pStmt->pConn->db, zSql, -1, 
                         &p->u.tab.pStmt, 0);
      sqlite3_free(zSql);
      break;
    }

    case TK_NULL:                 /* Initializing a NULL DS is a no-op */
      assert( p->u.null.isDone==0 );
      break;
  }
  return XJD1_OK;
}

/*
** Advance a data source to the next row.
** Return XJD1_DONE if the data source is empty and XJD1_ROW if
** the step results in a row of content being available.
*/
int xjd1DataSrcStep(DataSrc *p){
  int rc= XJD1_DONE;
  if( p==0 ) return XJD1_DONE;
  switch( p->eDSType ){
    case TK_COMMA: {
      rc = xjd1DataSrcStep(p->u.join.pRight);
      if( rc==XJD1_DONE ){
        xjd1DataSrcRewind(p->u.join.pRight);
        rc = xjd1DataSrcStep(p->u.join.pLeft);
      }
      break;
    }
    case TK_SELECT: {
      xjd1JsonFree(p->pValue);
      p->pValue = 0;
      rc = xjd1QueryStep(p->u.subq.q);
      p->pValue = xjd1QueryDoc(p->u.subq.q, 0);
      break;
    }
    case TK_ID: {
      rc = sqlite3_step(p->u.tab.pStmt);
      xjd1JsonFree(p->pValue);
      p->pValue = 0;
      if( rc==SQLITE_ROW ){
        const char *zJson = (const char*)sqlite3_column_text(p->u.tab.pStmt, 0);
        p->pValue = xjd1JsonParse(zJson, -1);
        rc = XJD1_ROW;
      }else{
        p->u.tab.eofSeen = 1;
        rc = XJD1_DONE;
      }
      break;
    }
    case TK_NULL: {
      rc = (p->u.null.isDone ? XJD1_DONE : XJD1_ROW);
      p->u.null.isDone = 1;
      break;
    }
  }
  return rc;
}

/*
** Return the document that this data source if the document if the AS
** name of the document is zDocName or if zDocName==0.  The AS name is
** important since a join might have multiple documents.
**
** xjd1JsonRef() has been called on the returned string.  The caller
** must invoke xjd1JsonFree().
*/
JsonNode *xjd1DataSrcDoc(DataSrc *p, const char *zDocName){
  JsonNode *pRes = 0;
  if( p==0 ) return 0;
  if( zDocName && p->zAs && 0==strcmp(p->zAs, zDocName) ){
    return xjd1JsonRef(p->pValue);
  }
  switch( p->eDSType ){
    case TK_COMMA: {
      pRes = xjd1DataSrcDoc(p->u.join.pLeft, zDocName);
      if( pRes==0 ) pRes = xjd1DataSrcDoc(p->u.join.pRight, zDocName);
      break;
    }
    case TK_SELECT: {
      assert( p->zAs );
      if( zDocName==0 ){
        pRes = xjd1JsonRef(p->pValue);
      }
      break;
    }
    case TK_ID: {
      if( zDocName==0 || (p->zAs==0 && strcmp(p->u.tab.zName, zDocName)==0) ){
        pRes = xjd1JsonRef(p->pValue);
      }
      break;
    }
    default: {
      pRes = xjd1JsonRef(p->pValue);
      break;
    }
  }
  return pRes;
}

/*
** Rewind a data source so that it is pointing at the first row.
** Return XJD1_DONE if the data source is empty and XJD1_ROW if
** the rewind results in a row of content being available.
*/
int xjd1DataSrcRewind(DataSrc *p){
  if( p==0 ) return XJD1_DONE;
  xjd1JsonFree(p->pValue);  p->pValue = 0;
  switch( p->eDSType ){
    case TK_COMMA: {
      xjd1DataSrcRewind(p->u.join.pLeft);
      xjd1DataSrcRewind(p->u.join.pRight);
      break;
    }
    case TK_SELECT: {
      xjd1QueryRewind(p->u.subq.q);
      break;
    }
    case TK_ID: {
      sqlite3_reset(p->u.tab.pStmt);
      return xjd1DataSrcStep(p);
    }
    case TK_NULL: {
      p->u.null.isDone = 0;
      return xjd1DataSrcStep(p);
    }
  }
  return XJD1_DONE;
}

/*
** The destructor for a Query object.
*/
int xjd1DataSrcClose(DataSrc *p){
  if( p==0 ) return XJD1_OK;
  xjd1JsonFree(p->pValue);  p->pValue = 0;
  switch( p->eDSType ){
    case TK_ID: {
      sqlite3_finalize(p->u.tab.pStmt);
      break;
    }
  }
  return XJD1_OK;
}
