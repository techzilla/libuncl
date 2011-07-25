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
  int rc = XJD1_OK;
  p->pQuery = pQuery;
  switch( p->eDSType ){
    case TK_COMMA: {
      xjd1DataSrcInit(p->u.join.pLeft, pQuery);
      xjd1DataSrcInit(p->u.join.pRight, pQuery);
      break;
    }
    case TK_SELECT: {
      xjd1QueryInit(p->u.subq.q, pQuery->pStmt, 0);
      break;
    }
    case TK_ID: {
      char *zSql = sqlite3_mprintf("SELECT x FROM \"%w\"", p->u.tab.zName);
      sqlite3_prepare_v2(pQuery->pStmt->pConn->db, zSql, -1, 
                         &p->u.tab.pStmt, 0);
      sqlite3_free(zSql);
      break;
    }
    case TK_FLATTENOP: {
      const char *zOp = (p->u.flatten.cOpName=='E' ? "EACH" : "FLATTEN");
      ExprItem *pItem = &p->u.flatten.pList->apEItem[0];
      xjd1DataSrcInit(p->u.flatten.pNext, pQuery);
      rc = xjd1FlattenExprInit(pItem->pExpr,p->u.flatten.pNext,&pItem->zAs,zOp);
      break;
    }
    case TK_NULL:                 /* Initializing a NULL DS is a no-op */
      assert( p->u.null.isDone==0 );
      break;
  }
  return rc;
}

static JsonNode *newIntValue(int i){
  JsonNode *pRet = xjd1JsonNew(0);
  pRet->eJType = XJD1_REAL;
  pRet->u.r = (double)i;
  return pRet;
}

static JsonNode *newStringValue(const char *z){
  JsonNode *pRet = xjd1JsonNew(0);
  pRet->eJType = XJD1_STRING;
  pRet->u.z = xjd1PoolDup(0, z, -1);
  return pRet;
}

static JsonNode *flattenedObject(
  JsonNode *pBase,
  JsonNode *pKey,
  JsonNode *pValue,
  const char *zAs
){
  JsonNode *pRet;                 /* Value to return */
  JsonNode *pKV;                  /* Value to return */

  pKV = xjd1JsonNew(0);
  pKV->eJType = XJD1_STRUCT;
  xjd1JsonInsert(pKV, "k", pKey);
  xjd1JsonInsert(pKV, "v", pValue);

  pRet = xjd1JsonEdit(xjd1JsonRef(pBase));
  assert( pRet->nRef==1 && pRet!=pBase );
  xjd1JsonInsert(pRet, zAs, pKV);
  return pRet;
}

static int valueIsEmpty(JsonNode *p){
  if( p && ( 
      (p->eJType==XJD1_ARRAY && p->u.ar.nElem>0)
   || (p->eJType==XJD1_STRUCT && p->u.st.pFirst!=0)
  )){
    return 0;
  }
  return 1;
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

      if( p->u.join.bStart==0 ){
        p->u.join.bStart = 1;
        rc = xjd1DataSrcStep(p->u.join.pLeft);
        if( rc!=XJD1_ROW ) break;
      }

      rc = xjd1DataSrcStep(p->u.join.pRight);
      if( rc==XJD1_DONE ){
        rc = xjd1DataSrcStep(p->u.join.pLeft);
        if( rc==XJD1_ROW ) {
          xjd1DataSrcRewind(p->u.join.pRight);
          rc = xjd1DataSrcStep(p->u.join.pRight);
        }
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

    case TK_FLATTENOP: {
      ExprItem *pItem = &p->u.flatten.pList->apEItem[0];

      xjd1JsonFree(p->pValue);
      p->pValue = 0;
      rc = XJD1_ROW;

      while( rc==XJD1_ROW && valueIsEmpty(p->u.flatten.pValue) ){
        rc = xjd1DataSrcStep(p->u.flatten.pNext);
        if( rc==XJD1_ROW ){
          p->u.flatten.pValue = xjd1ExprEval(pItem->pExpr);
          p->u.flatten.iIdx = 0;
        }
      }

      if( rc==XJD1_ROW ){
        JsonNode *pOut = 0;
        JsonNode *pVal = p->u.flatten.pValue;
        JsonNode *pBase = p->u.flatten.pNext->pValue;
        int iIdx = p->u.flatten.iIdx++;
        int bEof = 0;

        switch( pVal->eJType ){
          case XJD1_STRUCT: {
            int i;
            JsonStructElem *pElem = pVal->u.st.pFirst;
            for(i=0; i<iIdx; i++) pElem = pElem->pNext;
            bEof = (pElem->pNext==0);
            pOut = flattenedObject(pBase, newStringValue(pElem->zLabel), 
                xjd1JsonRef(pElem->pValue), pItem->zAs
            );
            break;
          }
          case XJD1_ARRAY: {
            assert( iIdx<pVal->u.ar.nElem );
            bEof = ( (iIdx+1)>=pVal->u.ar.nElem );
            pOut = flattenedObject(pBase, newIntValue(iIdx),
                xjd1JsonRef(pVal->u.ar.apElem[iIdx]), pItem->zAs
            );
            break;
          }
        }

        if( bEof ){
          p->u.flatten.pValue = 0;
          xjd1JsonFree(p->u.flatten.pValue);
        }
        p->pValue = pOut;
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
** Return the document that this data source is current pointing to
** if the AS name of the document is zDocName or if zDocName==0.
** The AS name is important since a join might have multiple documents.
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
    case TK_FLATTENOP:
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
** Rewind a data source so that the next call to DataSrcStep() will cause
** it to point to the first row.
*/
int xjd1DataSrcRewind(DataSrc *p){
  if( p==0 ) return XJD1_DONE;
  xjd1JsonFree(p->pValue);  p->pValue = 0;
  switch( p->eDSType ){
    case TK_COMMA: {
      p->u.join.bStart = 0;
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
      break;
    }
    case TK_FLATTENOP: {
      xjd1DataSrcRewind(p->u.flatten.pNext);
      xjd1JsonFree(p->u.flatten.pValue);
      p->u.flatten.pValue = 0;
      p->u.flatten.iIdx = 0;
      break;
    }
    case TK_NULL: {
      p->u.null.isDone = 0;
      break;
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
    case TK_FLATTENOP: {
      xjd1DataSrcClose(p->u.flatten.pNext);
      break;
    }
  }
  return XJD1_OK;
}

int xjd1DataSrcCount(DataSrc *p){
  int n = 1;
  if( p->eDSType==TK_COMMA ){
    n = xjd1DataSrcCount(p->u.join.pLeft) + xjd1DataSrcCount(p->u.join.pRight); 
  }
  return n;
}

static void cacheSaveRecursive(DataSrc *p, JsonNode ***papNode){
  if( p->eDSType==TK_COMMA ){
    cacheSaveRecursive(p->u.join.pLeft, papNode);
    cacheSaveRecursive(p->u.join.pRight, papNode);
  }else{
    xjd1JsonFree(**papNode);
    **papNode = xjd1JsonRef(p->pValue);
    (*papNode)++;
  }
}

void xjd1DataSrcCacheSave(DataSrc *p, JsonNode **apNode){
  JsonNode **pp = apNode;
  cacheSaveRecursive(p, &pp);
}

static int datasrcResolveRecursive(
  DataSrc *p, 
  int *piEntry, 
  const char *zDocname
){
  int ret = 0;
  if( p->eDSType==TK_COMMA ){
    ret = datasrcResolveRecursive(p->u.join.pLeft, piEntry, zDocname);
    if( 0==ret ){
      ret = datasrcResolveRecursive(p->u.join.pRight, piEntry, zDocname);
    }
  }else if( p->eDSType==TK_FLATTENOP ){
    ret = datasrcResolveRecursive(p->u.flatten.pNext, piEntry, zDocname);
  }else{
    if( (p->zAs && 0==strcmp(zDocname, p->zAs))
     || (p->zAs==0 && p->eDSType==TK_ID && strcmp(p->u.tab.zName, zDocname)==0)
    ){
      ret = *piEntry;
    }
    (*piEntry)++;
  }
  return ret;
}
int xjd1DataSrcResolve(DataSrc *p, const char *zDocname){
  int iEntry = 1;
  return datasrcResolveRecursive(p, &iEntry, zDocname);
}

static JsonNode *datasrcReadRecursive(
  DataSrc *p, 
  int *piEntry, 
  int iDoc
){
  JsonNode *pRet = 0;
  if( p->eDSType==TK_COMMA ){
    pRet = datasrcReadRecursive(p->u.join.pLeft, piEntry, iDoc);
    if( 0==pRet ){
      pRet = datasrcReadRecursive(p->u.join.pRight, piEntry, iDoc);
    }
  }else{
    if( *piEntry==iDoc ){
      pRet = xjd1JsonRef(p->pValue);
    }
    (*piEntry)++;
  }
  return pRet;
}
JsonNode *xjd1DataSrcRead(DataSrc *p, int iDoc){
  int iEntry = 1;
  assert( iDoc>=1 );
  return datasrcReadRecursive(p, &iEntry, iDoc);
}
