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
      char *zSql = sqlite3_mprintf("SELECT x FROM \"%.*w\"",
                            p->u.tab.name.n, p->u.tab.name.z);
      sqlite3_prepare_v2(pQuery->pStmt->pConn->db, zSql, -1, 
                         &p->u.tab.pStmt, 0);
      sqlite3_free(zSql);
      break;
    }
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
  }
  return rc;
}

/*
** Return the value after the most recent Step.  The returned JsonNode
** is managed by the DataSrc object will be freed by the next call to
** DataSrcStep(), DataSrcRewind(), or DataSrcClose().
*/
JsonNode *xjd1DataSrcValue(DataSrc *p){
  JsonNode *pOut = 0;
  if( p==0 ) return 0;
  switch( p->eDSType ){
    case TK_ID: {
      pOut = p->pValue;
      break;
    }
  }
  return pOut;
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
    case TK_ID: {
      sqlite3_reset(p->u.tab.pStmt);
      return xjd1DataSrcStep(p);
    }
  }
  return XJD1_DONE;
}

/*
** Return true if the data source is at the end of file
*/
int xjd1DataSrcEOF(DataSrc *p){
  return 1;
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
