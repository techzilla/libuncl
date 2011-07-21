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
  JsonNode **apKey;               /* Array of JSON objects */
  ResultItem *pNext;              /* Next element in list */
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

static int cmpResultItem(ResultItem *p1, ResultItem *p2, ExprList *pEList){
  int i;
  int c = 0;
  int nEItem = (pEList ? pEList->nEItem : 1);
  for(i=0; i<nEItem; i++){
    if( 0!=(c=xjd1JsonCompare(p1->apKey[i], p2->apKey[i])) ) break;
  }
  if( c && pEList ){
    char const *zDir = pEList->apEItem[i].zAs;
    if( zDir && zDir[0]=='D' ){
      c = c*-1;
    }
  }
  return c;
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
      int c = cmpResultItem(p1, p2, pEList);
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

static void sortResultList(ResultList *pList, ExprList *pEList){
  int i;                          /* Used to iterate through aList[] */
  ResultItem *aList[40];          /* Array of slots for merge sort */
  ResultItem *pHead;

  memset(aList, 0, sizeof(aList));
  pHead = pList->pItem;

  while( pHead ){
    ResultItem *pNext = pHead->pNext;
    pHead->pNext = 0;
    for(i=0; aList[i]; i++){
      assert( i<ArraySize(aList) );
      pHead = mergeResultItems(pEList, pHead, aList[i]);
      aList[i] = 0;
    }
    aList[i] = pHead;
    pHead = pNext;
  }

  pHead = aList[0];
  for(i=1; i<ArraySize(aList); i++){
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
  memset(pList, 0, sizeof(ResultList));
}

/*
** Called after statement parsing to initalize every Query object
** within the statement.
*/
int xjd1QueryInit(Query *pQuery, xjd1_stmt *pStmt, Query *pOuter){
  int rc;
  if( pQuery==0 ) return XJD1_OK;
  pQuery->pStmt = pStmt;
  pQuery->pOuter = pOuter;
  if( pQuery->eQType==TK_SELECT ){
    rc = xjd1ExprInit(pQuery->u.simple.pRes, pStmt, pQuery, 1);
    if( !rc ) rc = xjd1DataSrcInit(pQuery->u.simple.pFrom, pQuery);
    if( !rc ) rc = xjd1ExprInit(pQuery->u.simple.pWhere, pStmt, pQuery, 0);
    if( !rc ) rc = xjd1ExprListInit(pQuery->u.simple.pGroupBy, pStmt, pQuery,0);
    if( !rc ) rc = xjd1ExprInit(pQuery->u.simple.pHaving, pStmt, pQuery, 1);
    if( !rc ) rc = xjd1ExprListInit(pQuery->u.simple.pOrderBy, pStmt, pQuery,1);
    if( !rc ) rc = xjd1ExprInit(pQuery->u.simple.pLimit, pStmt, pQuery, 0);
    if( !rc ) rc = xjd1ExprInit(pQuery->u.simple.pOffset, pStmt, pQuery, 0);
    if( !rc && pQuery->u.simple.pGroupBy ){ 
      rc = xjd1AggregateInit(pStmt, pQuery, 0);
    }
  }else{
    rc = xjd1QueryInit(pQuery->u.compound.pLeft, pStmt, pOuter);
    if( !rc ) rc = xjd1QueryInit(pQuery->u.compound.pRight, pStmt, pOuter);
  }
  return rc;
}

/*
** Rewind a query so that it is pointing at the first row.
*/
int xjd1QueryRewind(Query *p){
  if( p==0 ) return XJD1_OK;
  if( p->eQType==TK_SELECT ){
    xjd1DataSrcRewind(p->u.simple.pFrom);
    p->eDocFrom = XJD1_FROM_DATASRC;
    p->bStarted = 0;
    clearResultList(&p->ordered);
    clearResultList(&p->grouped);
    clearResultList(&p->distincted);
    xjd1AggregateClear(p);
  }else{
    xjd1QueryRewind(p->u.compound.pLeft);
    p->u.compound.doneLeft = 0;
    xjd1QueryRewind(p->u.compound.pRight);
  }
  return XJD1_OK;
}

/*
** Advance to the next row of the TK_SELECT query passed as the first 
** argument, disregarding any ORDER BY, OFFSET or LIMIT clause.
**
** Return XJD1_ROW if there is such a row, or XJD1_DONE at EOF. Or return 
** an error code if an error occurs.
*/
static int selectStepWhered(Query *p){
  int rc;                         /* Return code */
  assert( p->eQType==TK_SELECT );
  do{
    rc = xjd1DataSrcStep(p->u.simple.pFrom);
  }while(
    rc==XJD1_ROW
    && (p->u.simple.pWhere!=0 && !xjd1ExprTrue(p->u.simple.pWhere))
  );
  return rc;
}

static int selectStepGrouped(Query *p){
  int rc;

  if( p->pAgg ){
    Aggregate *pAgg = p->pAgg;
    ExprList *pGroupBy = p->u.simple.pGroupBy;

    rc = XJD1_OK;
    if( pGroupBy==0 ){
      /* An aggregate query with no GROUP BY clause. If there is no GROUP BY,
      ** then exactly one row is returned, which makes ORDER BY and DISTINCT 
      ** no-ops. And it is not possible to have a HAVING clause without a
      ** GROUP BY, so no need to worry about that either. 
      */
      assert( p->u.simple.pHaving==0 );

      if( p->grouped.pPool==0 ){
        JsonNode **apSrc;
        int nSrc;
        Pool *pPool;

        pPool = p->grouped.pPool = xjd1PoolNew();
        if( !pPool ) return XJD1_NOMEM;

        p->grouped.nKey = nSrc = xjd1DataSrcCount(p->u.simple.pFrom);
        apSrc = (JsonNode **)xjd1PoolMallocZero(pPool, nSrc*sizeof(JsonNode *));
        if( !apSrc ) return XJD1_NOMEM;

        /* Call the xStep() of each aggregate in the query for each row 
        ** matched by the query WHERE clause. */
        while( rc==XJD1_OK && XJD1_ROW==(rc = selectStepWhered(p) ) ){
          rc = xjd1AggregateStep(pAgg);
          xjd1DataSrcCacheSave(p->u.simple.pFrom, apSrc);
        }
        assert( rc!=XJD1_OK );
        if( rc==XJD1_DONE ){
          rc = addToResultList(&p->grouped, apSrc);
        }
        if( rc==XJD1_OK ){
          rc = xjd1AggregateFinalize(pAgg);
        }
        if( rc==XJD1_OK ){
          rc = XJD1_ROW;
          p->eDocFrom = XJD1_FROM_GROUPED;
        }

      }else{
        rc = XJD1_DONE;
      }
      
      /* TODO: If this is a top-level query, then xQueryDoc(p, 0) will be
      ** called exactly once to retrieve the query result. When any 
      ** aggregate expressions within the query result are evaluated, the 
      ** xFinal function is evaluated. But this will only work once - if
      ** xQueryDoc(p, 0) is called more than once it will break. Find out if
      ** this is a problem!
      */

    }else{

      /* An aggregate with a GROUP BY clause. There may also be a DISTINCT
      ** qualifier.
      **
      ** Use a ResultList to sort all the rows matched by the WHERE
      ** clause. The apKey[] array consists of each of the expressions
      ** in the GROUP BY clause, followed by each document in the FROM
      ** clause. */
      do {
        ResultItem *pItem;
  
        if( p->grouped.pPool==0 ){
          DataSrc *pFrom = p->u.simple.pFrom;
          JsonNode **apKey;
          int nByte;
          Pool *pPool;
  
          /* Allocate the memory pool for this ResultList. And apKey. */
          pPool = p->grouped.pPool = xjd1PoolNew();
          p->grouped.nKey = pGroupBy->nEItem + xjd1DataSrcCount(pFrom);
          if( !pPool ) return XJD1_NOMEM;
          nByte = p->grouped.nKey * sizeof(JsonNode *);
          apKey = (JsonNode **)xjd1PoolMallocZero(pPool, nByte);
          if( !apKey ) return XJD1_NOMEM;
  
          while( rc==XJD1_OK && XJD1_ROW==(rc = selectStepWhered(p) ) ){
            int i;
            for(i=0; i<pGroupBy->nEItem; i++){
              apKey[i] = xjd1ExprEval(pGroupBy->apEItem[i].pExpr);
            }
            xjd1DataSrcCacheSave(pFrom, &apKey[i]);
            rc = addToResultList(&p->grouped, apKey);
            memset(apKey, 0, nByte);
          }
          if( rc!=XJD1_DONE ) return rc;
          sortResultList(&p->grouped, pGroupBy);
        }else{
          popResultList(&p->grouped);
        }
  
        p->eDocFrom = XJD1_FROM_GROUPED;
        pItem = p->grouped.pItem;
        if( pItem==0 ){
          rc = XJD1_DONE;
        }else{
          while( 1 ){
            ResultItem *pNext = pItem->pNext;
            rc = xjd1AggregateStep(pAgg);
            if( rc || !pNext || cmpResultItem(pItem, pNext, pGroupBy) ){
              break;
            }
            popResultList(&p->grouped);
            pItem = p->grouped.pItem;
          }
          if( XJD1_OK==rc && XJD1_OK==(rc = xjd1AggregateFinalize(pAgg)) ){
            rc = XJD1_ROW;
          }
        }

      }while( rc==XJD1_ROW 
           && p->u.simple.pHaving && !xjd1ExprTrue(p->u.simple.pHaving)
      );
    }

  }else{
    /* Non-aggregate query */
    rc = selectStepWhered(p);
  }

  return rc;
}

static int selectStepDistinct(Query *p){
  int rc;
  if( p->u.simple.isDistinct ){
    if( p->distincted.pPool==0 ){
      Pool *pPool;
      JsonNode **apKey;
      int nKey;

      nKey = 1 + xjd1DataSrcCount(p->u.simple.pFrom);
      pPool = p->distincted.pPool = xjd1PoolNew();
      if( !pPool ) return XJD1_NOMEM;
      p->distincted.nKey = nKey;
      apKey = xjd1PoolMallocZero(pPool, nKey * sizeof(JsonNode *));
      if( !apKey ) return XJD1_NOMEM;

      while( XJD1_ROW==(rc = selectStepGrouped(p) ) ){
        apKey[0] = xjd1QueryDoc(p, 0);
        xjd1DataSrcCacheSave(p->u.simple.pFrom, &apKey[1]);
        rc = addToResultList(&p->distincted, apKey);
        memset(apKey, 0, nKey * sizeof(JsonNode *));
        if( rc!=XJD1_OK ) break;
      }
      if( rc==XJD1_DONE ){
        sortResultList(&p->distincted, 0);
        p->eDocFrom = XJD1_FROM_DISTINCTED;
        rc = XJD1_ROW;
      }
    }else{
      JsonNode *pPrev = xjd1JsonRef(p->distincted.pItem->apKey[0]);
      do{
        popResultList(&p->distincted);
      }while( p->distincted.pItem 
           && 0==xjd1JsonCompare(pPrev, p->distincted.pItem->apKey[0])
      );
      rc = p->distincted.pItem ? XJD1_ROW : XJD1_DONE;
    }
  }else{
    rc = selectStepGrouped(p);


  }
  return rc;
}

/*
** Advance to the next row of the TK_SELECT query passed as the first 
** argument, disregarding any OFFSET or LIMIT clause.
**
** Return XJD1_ROW if there is such a row, or XJD1_EOF if there is not. Or 
** return an error code if an error occurs.
*/
static int selectStepOrdered(Query *p){
  int rc = XJD1_OK;               /* Return Code */

  if( p->u.simple.pOrderBy ){

    /* A non-aggregate with an ORDER BY clause. */
    if( p->ordered.pPool==0 ){
      int nKey = p->u.simple.pOrderBy->nEItem + 1;
      ExprList *pOrderBy = p->u.simple.pOrderBy;
      Pool *pPool;
      JsonNode **apKey;

      p->ordered.nKey = nKey;
      pPool = p->ordered.pPool = xjd1PoolNew();
      if( !pPool ) return XJD1_NOMEM;
      apKey = xjd1PoolMallocZero(pPool, nKey * sizeof(JsonNode *));
      if( !apKey ) return XJD1_NOMEM;

      while( XJD1_ROW==(rc = selectStepDistinct(p) ) ){
        int i;
        for(i=0; i<pOrderBy->nEItem; i++){
          apKey[i] = xjd1ExprEval(pOrderBy->apEItem[i].pExpr);
        }
        apKey[i] = xjd1QueryDoc(p, 0);

        rc = addToResultList(&p->ordered, apKey);
        if( rc!=XJD1_OK ) break;
      }
      if( rc!=XJD1_DONE ) return rc;

      sortResultList(&p->ordered, p->u.simple.pOrderBy);
      p->eDocFrom = XJD1_FROM_ORDERED;
    }else{
      assert( p->eDocFrom==XJD1_FROM_ORDERED );
      popResultList(&p->ordered);
    }

    rc = p->ordered.pItem ? XJD1_ROW : XJD1_DONE;
  }else{
    /* No ORDER BY clause. */
    rc = selectStepDistinct(p);
  }

  return rc;
}

/*
** Advance a query to the next row.  Return XDJ1_DONE if there is no
** next row, or XJD1_ROW if the step was successful.
*/
int xjd1QueryStep(Query *p){
  int rc = XJD1_ROW;
  if( p==0 ) return XJD1_DONE;
  if( p->eQType==TK_SELECT ){

    /* Calculate the values, if any, of the LIMIT and OFFSET clauses.
    **
    ** TBD: Should this throw an exception if the result of evaluating
    ** either of these clauses cannot be converted to a number?
    */
    if( p->bStarted==0 ){
      if( p->u.simple.pOffset ){
        double rOffset;
        JsonNode *pOffset;

        pOffset = xjd1ExprEval(p->u.simple.pOffset);
        if( 0==xjd1JsonToReal(pOffset, &rOffset) ){
          int nOffset;
          for(nOffset=(int)rOffset; nOffset>0; nOffset--){
            rc = selectStepOrdered(p);
            if( rc!=XJD1_ROW ) break;
          }
        }
        xjd1JsonFree(pOffset);
      }

      p->nLimit = -1;
      if( p->u.simple.pLimit ){
        double rLimit;
        JsonNode *pLimit;

        pLimit = xjd1ExprEval(p->u.simple.pLimit);
        if( 0==xjd1JsonToReal(pLimit, &rLimit) ){
          p->nLimit = (int)rLimit;
        }
        xjd1JsonFree(pLimit);
      }
    }

    if( rc==XJD1_ROW ){
      if( p->nLimit==0 ){
        rc = XJD1_DONE;
      }else{
        rc = selectStepOrdered(p);
        if( p->nLimit>0 ) p->nLimit--;
      }
    }
    p->bStarted = 1;
  }else{
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
      switch( p->eDocFrom ){
        case XJD1_FROM_ORDERED:
          assert( zDocName==0 && p->ordered.pItem );
          pOut = xjd1JsonRef(p->ordered.pItem->apKey[p->ordered.nKey-1]);
          break;

        case XJD1_FROM_DISTINCTED:
          if( zDocName==0 ){
            pOut = xjd1JsonRef(p->distincted.pItem->apKey[0]);
          }else{
            JsonNode **apSrc = &p->distincted.pItem->apKey[1];
            pOut = xjd1DataSrcCacheRead(p->u.simple.pFrom, apSrc, zDocName);
          }
          break;

        case XJD1_FROM_GROUPED:
          if( zDocName==0 && p->u.simple.pRes ){
            pOut = xjd1ExprEval(p->u.simple.pRes);
          }else{
            JsonNode **apSrc = p->grouped.pItem->apKey;
            if( p->u.simple.pGroupBy ){
              apSrc = &apSrc[p->u.simple.pGroupBy->nEItem];
            }
            pOut = xjd1DataSrcCacheRead(p->u.simple.pFrom, apSrc, zDocName);
          }
          break;

        case XJD1_FROM_DATASRC:
          if( zDocName==0 && p->u.simple.pRes ){
            pOut = xjd1ExprEval(p->u.simple.pRes);
          }else{
            pOut = xjd1DataSrcDoc(p->u.simple.pFrom, zDocName);
          }
          break;
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
    clearResultList(&pQuery->ordered);
    clearResultList(&pQuery->grouped);
    clearResultList(&pQuery->distincted);
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

