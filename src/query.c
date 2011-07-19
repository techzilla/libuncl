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
** This file contains code used to implement query processing.
*/
#include "xjd1Int.h"


struct ResultItem {
  JsonNode **apKey;               /* List of JSON objects - the sort key */
  ResultItem *pNext;           /* Next element in list */
};

static int addToResultList(
  ResultList *pList,              /* List to append to */
  JsonNode **apKey                /* Array of values to add to list */
){
  ResultItem *pNew;               /* Newly allocated ResultItem */
  int nByte;                      /* Bytes to allocate for new node */
  int i;                          /* Used to iterate through apKey[] */

  nByte = sizeof(ResultItem) + sizeof(JsonNode*) * pList->nKey;
  pNew = (ResultItem *)xjd1PoolMalloc(pList->pPool, nByte);
  if( !pNew ){
    for(i=0; i<pList->nKey; i++){
      xjd1JsonFree(apKey[i]);
    }
    return XJD1_NOMEM;
  }

  pNew->apKey = (JsonNode **)&pNew[1];
  memcpy(pNew->apKey, apKey, pList->nKey * sizeof(JsonNode *));
  pNew->pNext = pList->pItem;
  pList->pItem = pNew;

  return XJD1_OK;
}

static ResultItem *mergeResultItems(
  ExprList *pEList,              /* Used for ASC/DESC of each key */
  ResultItem *p1,                /* First list to merge */
  ResultItem *p2                 /* Second list to merge */
){
  ResultItem *pRet = 0;
  ResultItem **ppNext;

  ppNext = &pRet;
  while( p1 || p2 ){
    if( !p1 ){
      *ppNext = p2;
      p2 = 0;
    }else if( !p2 ){
      *ppNext = p1;
      p1 = 0;
    }else{
      char const *zDir;
      int c;
      int i;

      for(i=0; i<pEList->nEItem; i++){
        if( 0!=(c=xjd1JsonCompare(p1->apKey[i], p2->apKey[i])) ) break;
      }
      zDir = pEList->apEItem[i].zAs;
      if( zDir && zDir[0]=='D' ){
        c = c*-1;
      }

      if( c<=0 ){
        *ppNext = p1;
        ppNext = &p1->pNext;
        p1 = p1->pNext;
      }else{
        *ppNext = p2;
        ppNext = &p2->pNext;
        p2 = p2->pNext;
      }
    }
  }

  return pRet;
}

#define N_BUCKET 40
static void sortResultList(ResultList *pList, ExprList *pEList){
  int i;
  ResultItem *aList[40];

  ResultItem *pNext;
  ResultItem *pHead;

  memset(aList, 0, sizeof(aList));
  pHead = pList->pItem;

  while( pHead ){
    pNext = pHead->pNext;
    pHead->pNext = 0;
    for(i=0; aList[i]; i++){
      assert( i<N_BUCKET );
      pHead = mergeResultItems(pEList, pHead, aList[i]);
      aList[i] = 0;
    }
    aList[i] = pHead;
    pHead = pNext;
  }

  pHead = aList[0];
  for(i=1; i<N_BUCKET; i++){
    pHead = mergeResultItems(pEList, pHead, aList[i]);
  }

  pList->pItem = pHead;
}

static void popResultList(ResultList *pList){
  ResultItem *pItem;
  int i;

  pItem = pList->pItem;
  pList->pItem = pItem->pNext;
  for(i=0; i<pList->nKey; i++){
    xjd1JsonFree(pItem->apKey[i]);
  }
}

static void clearResultList(ResultList *pList){
  while( pList->pItem ) popResultList(pList);
  xjd1PoolDelete(pList->pPool);
  pList->pPool = 0;
}

/*
** Called after statement parsing to initalize every Query object
** within the statement.
*/
int xjd1QueryInit(Query *pQuery, xjd1_stmt *pStmt, Query *pOuter){
  if( pQuery==0 ) return XJD1_OK;
  pQuery->pStmt = pStmt;
  pQuery->pOuter = pOuter;
  if( pQuery->eQType==TK_SELECT ){
    xjd1ExprInit(pQuery->u.simple.pRes, pStmt, pQuery);
    xjd1DataSrcInit(pQuery->u.simple.pFrom, pQuery);
    xjd1ExprInit(pQuery->u.simple.pWhere, pStmt, pQuery);
    xjd1ExprListInit(pQuery->u.simple.pGroupBy, pStmt, pQuery);
    xjd1ExprInit(pQuery->u.simple.pHaving, pStmt, pQuery);
    xjd1ExprListInit(pQuery->u.simple.pOrderBy, pStmt, pQuery);
    xjd1ExprInit(pQuery->u.simple.pLimit, pStmt, pQuery);
    xjd1ExprInit(pQuery->u.simple.pOffset, pStmt, pQuery);
  }else{
    xjd1QueryInit(pQuery->u.compound.pLeft, pStmt, pOuter);
    xjd1QueryInit(pQuery->u.compound.pRight, pStmt, pOuter);
  }
  return XJD1_OK;
}

/*
** Rewind a query so that it is pointing at the first row.
*/
int xjd1QueryRewind(Query *p){
  if( p==0 ) return XJD1_OK;
  if( p->eQType==TK_SELECT ){
    xjd1DataSrcRewind(p->u.simple.pFrom);
    p->bUseResultList = 0;
    clearResultList(&p->result);
  }else{
    xjd1QueryRewind(p->u.compound.pLeft);
    p->u.compound.doneLeft = 0;
    xjd1QueryRewind(p->u.compound.pRight);
  }
  return XJD1_OK;
}

static int selectStepUnordered(Query *p){
  int rc;                         /* Return code */
  do{
    rc = xjd1DataSrcStep(p->u.simple.pFrom);
  }while(
    rc==XJD1_ROW
    && (p->u.simple.pWhere!=0 && !xjd1ExprTrue(p->u.simple.pWhere))
  );
  return rc;
}

/*
** Advance a query to the next row.  Return XDJ1_DONE if there is no
** next row, or XJD1_ROW if the step was successful.
*/
int xjd1QueryStep(Query *p){
  int rc;
  if( p==0 ) return XJD1_DONE;
  if( p->eQType==TK_SELECT ){

    if( p->u.simple.pOrderBy ){
      /* There is an ORDER BY clause. */
      if( p->bUseResultList==0 ){
        int nKey = p->u.simple.pOrderBy->nEItem + 1;
        ExprList *pOrderBy = p->u.simple.pOrderBy;
        Pool *pPool;
        JsonNode **apKey;

        p->result.nKey = nKey;
        pPool = p->result.pPool = xjd1PoolNew();
        if( !pPool ) return XJD1_NOMEM;
        apKey = xjd1PoolMallocZero(pPool, nKey * sizeof(JsonNode *));
        if( !apKey ) return XJD1_NOMEM;

        while( XJD1_ROW==(rc = selectStepUnordered(p) ) ){
          int i;
          for(i=0; i<pOrderBy->nEItem; i++){
            apKey[i] = xjd1ExprEval(pOrderBy->apEItem[i].pExpr);
          }
          apKey[i] = xjd1QueryDoc(p, 0);

          rc = addToResultList(&p->result, apKey);
          if( rc!=XJD1_OK ) break;
        }
        if( rc!=XJD1_DONE ) return rc;

        sortResultList(&p->result, p->u.simple.pOrderBy);
        p->bUseResultList = 1;
      }else{
        popResultList(&p->result);
      }

      rc = p->result.pItem ? XJD1_ROW : XJD1_DONE;
    }else{
      rc = selectStepUnordered(p);
    }

  }else{
    rc = XJD1_ROW;
    if( !p->u.compound.doneLeft ){
      rc = xjd1QueryStep(p->u.compound.pLeft);
      if( rc==XJD1_DONE ) p->u.compound.doneLeft = 1;
    }
    if( p->u.compound.doneLeft ){
      rc = xjd1QueryStep(p->u.compound.pRight);
    }
  }
  return rc;
}

/*
** Return a document currently referenced by a query.  If zDocName==0 then
** return the constructed result set of the query.
**
** The caller must invoke JsonFree() when it is done with this value.
*/
JsonNode *xjd1QueryDoc(Query *p, const char *zDocName){
  JsonNode *pOut = 0;
  if( p ){
    if( p->eQType==TK_SELECT ){
      if( p->bUseResultList ){
        assert( zDocName==0 && p->result.pItem );
        pOut = xjd1JsonRef(p->result.pItem->apKey[p->result.nKey-1]);
      }else if( zDocName==0 && p->u.simple.pRes ){
        pOut = xjd1ExprEval(p->u.simple.pRes);
      }else{
        pOut = xjd1DataSrcDoc(p->u.simple.pFrom, zDocName);
      }
    }else if( !p->u.compound.doneLeft ){
      pOut = xjd1QueryDoc(p->u.compound.pLeft, zDocName);
    }else{
      pOut = xjd1QueryDoc(p->u.compound.pRight, zDocName);
    }
    if( pOut==0 && zDocName ){
      pOut = xjd1QueryDoc(p->pOuter, zDocName);
    }
  }
  return pOut;
}


/*
** The destructor for a Query object.
*/
int xjd1QueryClose(Query *pQuery){
  int rc = XJD1_OK;
  if( pQuery==0 ) return rc;
  if( pQuery->eQType==TK_SELECT ){
    clearResultList(&pQuery->result);
    xjd1ExprClose(pQuery->u.simple.pRes);
    xjd1DataSrcClose(pQuery->u.simple.pFrom);
    xjd1ExprClose(pQuery->u.simple.pWhere);
    xjd1ExprListClose(pQuery->u.simple.pGroupBy);
    xjd1ExprClose(pQuery->u.simple.pHaving);
    xjd1ExprListClose(pQuery->u.simple.pOrderBy);
    xjd1ExprClose(pQuery->u.simple.pLimit);
    xjd1ExprClose(pQuery->u.simple.pOffset);
  }else{
    xjd1QueryClose(pQuery->u.compound.pLeft);
    xjd1QueryClose(pQuery->u.compound.pRight);
  }
  return rc;
}

