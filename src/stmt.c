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
  Command *pCmd;
  int rc;

  *pN = strlen(zStmt);
  *ppNew = p = malloc( sizeof(*p) );
  if( p==0 ) return XJD1_NOMEM;
  memset(p, 0, sizeof(*p));
  p->pConn = pConn;
  p->pNext = pConn->pStmt;
  pConn->pStmt = p;
  p->zCode = xjd1PoolDup(&p->sPool, zStmt, -1);
  if( pN==0 ) pN = &dummy;
  rc = xjd1RunParser(pConn, p, p->zCode, pN);
  pCmd = p->pCmd;
  if( pCmd ){
    switch( pCmd->eCmdType ){
      case TK_SELECT: {
        xjd1QueryInit(pCmd->u.q.pQuery, p, 0);
        break;
      }
      case TK_INSERT: {
        xjd1QueryInit(pCmd->u.ins.pQuery, p, 0);
        break;
      }
    }
  }
  return rc;
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
  Command *pCmd;
  if( pStmt==0 ) return XJD1_OK;
  pStmt->isDying = 1;
  if( pStmt->nRef>0 ) return XJD1_OK;

  pCmd = pStmt->pCmd;
  if( pCmd ){
    switch( pCmd->eCmdType ){
      case TK_SELECT: {
        xjd1QueryClose(pCmd->u.q.pQuery);
        break;
      }
      case TK_INSERT: {
        xjd1QueryClose(pCmd->u.ins.pQuery);
        break;
      }
    }
  }

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
  Command *pCmd;
  int rc = XJD1_DONE;
  if( pStmt==0 ) return rc;
  pCmd = pStmt->pCmd;
  if( pCmd==0 ) return rc;
  switch( pCmd->eCmdType ){
    case TK_CREATEDATASET: {
      char *zSql;
      int res;
      char *zErr = 0;
      zSql = sqlite3_mprintf("CREATE TABLE %s \"%!.*w\"(x)",
                 pCmd->u.crtab.ifExists ? "IF NOT EXISTS" : "",
                 pCmd->u.crtab.name.n, pCmd->u.crtab.name.z);
      res = sqlite3_exec(pStmt->pConn->db, zSql, 0, 0, &zErr);
      if( zErr ){
        xjd1Error(pStmt->pConn, XJD1_ERROR, "%s", zErr);
        sqlite3_free(zErr);
        rc = XJD1_ERROR;
      }
      sqlite3_free(zSql);
      break;
    }
    case TK_DROPDATASET: {
      char *zSql;
      int res;
      char *zErr = 0;
      zSql = sqlite3_mprintf("DROP TABLE %s \"%!.*w\"",
                 pCmd->u.crtab.ifExists ? "IF EXISTS" : "",
                 pCmd->u.crtab.name.n, pCmd->u.crtab.name.z);
      res = sqlite3_exec(pStmt->pConn->db, zSql, 0, 0, &zErr);
      if( zErr ){
        xjd1Error(pStmt->pConn, XJD1_ERROR, "%s", zErr);
        sqlite3_free(zErr);
        rc = XJD1_ERROR;
      }
      sqlite3_free(zSql);
      break;
    }
    case TK_INSERT: {
      JsonNode *pNode;
      String json;
      int res;
      char *zErr;
      char *zSql;
      if( pCmd->u.ins.pQuery ){
        xjd1Error(pStmt->pConn, XJD1_ERROR, 
                 "INSERT INTO ... SELECT not yet implemented");
        break;
      }
      pNode = xjd1JsonParse(pCmd->u.ins.jvalue.z, pCmd->u.ins.jvalue.n);
      if( pNode==0 ){
        xjd1Error(pStmt->pConn, XJD1_ERROR,
                  "malformed JSON");
        break;
      }
      xjd1StringInit(&json,0,0);
      xjd1JsonRender(&json, pNode);
      xjd1JsonFree(pNode);
      zSql = sqlite3_mprintf("INSERT INTO \"%.*w\" VALUES('%q')",
                  pCmd->u.ins.name.n, pCmd->u.ins.name.z,
                  xjd1StringText(&json));
      xjd1StringClear(&json);
      res = sqlite3_exec(pStmt->pConn->db, zSql, 0, 0, &zErr);
      if( zErr ){
        xjd1Error(pStmt->pConn, XJD1_ERROR, "%s", zErr);
        sqlite3_free(zErr);
        rc = XJD1_ERROR;
      }
      sqlite3_free(zSql);
      break;
    }
    case TK_SELECT: {
      rc = xjd1QueryStep(pStmt->pCmd->u.q.pQuery);
      break;
    }
    case TK_PRAGMA: {
      rc = xjd1PragmaStep(pStmt);
      break;
    }
  }
  return rc;
}

/*
** Rewind a prepared statement back to the beginning.
*/
int xjd1_stmt_rewind(xjd1_stmt *pStmt){
  Command *pCmd = pStmt->pCmd;
  if( pCmd ){
    switch( pCmd->eCmdType ){
      case TK_SELECT: {
        xjd1QueryRewind(pCmd->u.q.pQuery);
        break;
      }
      case TK_INSERT: {
        xjd1QueryRewind(pCmd->u.ins.pQuery);
        break;
      }
    }
  }
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
