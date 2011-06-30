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
** Code to evaluate an UPDATE statement
*/
#include "xjd1Int.h"


/*
** Perform an edit on a JSON value.  Return the document after the change.
*/
static JsonNode *reviseOneField(
  JsonNode *pDoc,        /* The document to be edited */
  Expr *pLvalue,         /* Definition of field in document to be changed */
  Expr *pValue           /* New value for the field */
){
  return pDoc;
}


/*
** Evaluate an UPDATE.
*/
int xjd1UpdateStep(xjd1_stmt *pStmt){
  Command *pCmd = pStmt->pCmd;
  int rc = XJD1_OK;
  int nUpdate = 0;
  sqlite3 *db = pStmt->pConn->db;
  sqlite3_stmt *pQuery, *pReplace;
  char *zSql;

  assert( pCmd!=0 );
  assert( pCmd->eCmdType==TK_UPDATE );
  if( pCmd->u.update.pUpsert ){
    sqlite3_exec(db, "BEGIN", 0, 0, 0);
  }
  zSql = sqlite3_mprintf("SELECT rowid, x FROM \"%w\"", pCmd->u.update.zName);
  sqlite3_prepare_v2(db, zSql, -1, &pQuery, 0);
  sqlite3_free(zSql);
  zSql = sqlite3_mprintf("UPDATE \"%w\" SET x=?1 WHERE rowid=?2",
                         pCmd->u.update.zName);
  sqlite3_prepare_v2(db, zSql, -1, &pReplace, 0);
  sqlite3_free(zSql);
  if( pQuery && pReplace ){
    while( SQLITE_ROW==sqlite3_step(pQuery) ){
      const char *zJson = (const char*)sqlite3_column_text(pQuery, 1);
      pStmt->pDoc = xjd1JsonParse(zJson, -1);
      if( xjd1ExprTrue(pCmd->u.del.pWhere) ){
        JsonNode *pNewDoc;  /* Revised document content */
        ExprList *pChng;    /* List of changes */
        String jsonNewDoc;  /* Text rendering of revised document */
        int i, n;

        pNewDoc  = xjd1JsonRef(pStmt->pDoc);
        pChng = pCmd->u.update.pChng;
        n = pChng->nEItem;
        for(i=0; i<n-1; i += 2){
          Expr *pLvalue = pChng->apEItem[i].pExpr;
          Expr *pExpr = pChng->apEItem[i+1].pExpr;
          pNewDoc = reviseOneField(pNewDoc, pLvalue, pExpr);
        }
        xjd1StringInit(&jsonNewDoc, 0, 0);
        xjd1JsonRender(&jsonNewDoc, pNewDoc);
        sqlite3_bind_int64(pReplace, 1, sqlite3_column_int64(pQuery, 0));
        sqlite3_bind_text(pReplace, 2, xjd1StringText(&jsonNewDoc), -1,
                          SQLITE_STATIC);
        sqlite3_step(pReplace);
        sqlite3_reset(pReplace);
        xjd1StringClear(&jsonNewDoc);
        xjd1JsonFree(pNewDoc);
        nUpdate++;
      }
      xjd1JsonFree(pStmt->pDoc);
      pStmt->pDoc = 0;
    }  
  }
  sqlite3_finalize(pQuery);
  sqlite3_finalize(pReplace);

  if( pCmd->u.update.pUpsert ){
    if( nUpdate==0 ){
      JsonNode *pToIns;
      String jsonToIns;
      pToIns = xjd1ExprEval(pCmd->u.update.pUpsert);
      xjd1StringInit(&jsonToIns, 0, 0); 
      xjd1JsonRender(&jsonToIns, pToIns);
      xjd1JsonFree(pToIns);
      zSql = sqlite3_mprintf(
           "INSERT INTO \"%w\" VALUES('%q')",
           pCmd->u.update.zName, xjd1StringText(&jsonToIns));
      xjd1StringClear(&jsonToIns);
      sqlite3_exec(db, zSql, 0, 0, 0);
      sqlite3_free(zSql);
    }
    sqlite3_exec(db, "COMMIT", 0, 0, 0);
  }
  return rc;
}
