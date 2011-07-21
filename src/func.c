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
*/
#include "xjd1Int.h"

struct Function {
  int nArg;
  const char *zName;

  JsonNode *(*xFunc)(int nArg, JsonNode **apArg);
  int (*xStep)(int nArg, JsonNode **apArg, void **);
  JsonNode *(*xFinal)(void *);
};

/*
** Implementation of scalar function "length(x)".
*/
static JsonNode *xLength(int nArg, JsonNode **apArg){
  JsonNode *pRet;
  JsonNode *pStr;
  int nRet;
  assert( nArg==1 );

  pStr = apArg[0];
  if( pStr->eJType==XJD1_STRING ){
    nRet = xjd1Strlen30(pStr->u.z);
  }else{
    String str;

    xjd1StringInit(&str, 0, 0);
    xjd1JsonToString(pStr, &str);
    nRet = str.nUsed;
    xjd1StringClear(&str);
  }

  pRet = xjd1JsonNew(0);
  pRet->eJType = XJD1_REAL;
  pRet->u.r = (double)nRet;

  return pRet;
}

static int xCountStep(int nArg, JsonNode **apArg, void **pp){
  int nCount = (int)*pp;
  nCount++;
  *pp = (void *)nCount;
  return XJD1_OK;
}

static JsonNode *xCountFinal(void *p){
  JsonNode *pRet;
  pRet = xjd1JsonNew(0);
  pRet->eJType = XJD1_REAL;
  pRet->u.r = (double)(int)p;
  return pRet;
}


int xjd1AggregateInit(xjd1_stmt *pStmt, Query *pQuery, Expr *p){
  Aggregate *pAgg = pQuery->pAgg;

  if( pAgg==0 ){
    pAgg = (Aggregate *)xjd1PoolMallocZero(&pStmt->sPool, sizeof(Aggregate));
    if( pAgg==0 ) return XJD1_NOMEM;
    pQuery->pAgg = pAgg;
  }

  if( p ){
    static const int ARRAY_ALLOC_INCR = 8;

    if( (pAgg->nExpr % ARRAY_ALLOC_INCR)==0 ){
      int nByte;                    /* Size of new allocation in bytes */
      int nCopy;                    /* Size of old allocation in bytes */
      AggExpr *aNew;                /* Pointer to new allocation */
     
      nByte = (pAgg->nExpr + ARRAY_ALLOC_INCR) * sizeof(AggExpr);
      nCopy = pAgg->nExpr * sizeof(AggExpr);
      aNew = (AggExpr *)xjd1PoolMallocZero(&pStmt->sPool, nByte);
  
      if( aNew==0 ) return XJD1_NOMEM;
      memcpy(aNew, pAgg->aAggExpr, nCopy);
      pAgg->aAggExpr = aNew;
    }
  
    p->u.func.iAgg = pAgg->nExpr;
    pAgg->aAggExpr[pAgg->nExpr++].pExpr = p;
  }

  return XJD1_OK;
}

/*
** The expression passed as the first argument is of type TK_FUNCTION.
** This function initializes the expression object. If successful, XJD1_OK
** is returned. Otherwise, an error code is returned and an error left in
** the pStmt statement handle.
*/
int xjd1FunctionInit(Expr *p, xjd1_stmt *pStmt, Query *pQuery, int bAggOk){
  char *zName;
  int nArg;
  int nByte;
  int i;

  static Function aFunc[] = {
    {  1, "length", xLength, 0, 0 },
    { -1, "count", 0, xCountStep, xCountFinal },
  };

  assert( p->eType==TK_FUNCTION && p->eClass==XJD1_EXPR_FUNC );
  assert( pQuery || bAggOk==0 );

  zName = p->u.func.zFName;
  nArg = p->u.func.args->nEItem;

  for(i=0; i<ArraySize(aFunc); i++){
    Function *pFunc = &aFunc[i];
    assert( (pFunc->xFunc==0)==(pFunc->xStep && pFunc->xFinal) );
    if( !strcmp(pFunc->zName, zName) && (nArg==pFunc->nArg || 0>pFunc->nArg) ){
      if( pFunc->xStep ){
        int rc;
        if( bAggOk==0 ){
          const char *zErrMsg = "illegal use of aggregate function: %s";
          xjd1StmtError(pStmt, XJD1_ERROR, zErrMsg, zName);
          return XJD1_ERROR;
        }

        /* Add this aggregate function to the Query.pAgg->apExpr[] array. */
        rc = xjd1AggregateInit(pStmt, pQuery, p);
        if( rc!=XJD1_OK ) return rc;
      }
      p->u.func.pFunction = pFunc;
      break;
    }
  }

  if( !p->u.func.pFunction ){
    xjd1StmtError(pStmt, XJD1_ERROR, "no such function: %s", zName);
    return XJD1_ERROR;
  }

  nByte = sizeof(JsonNode *) * nArg;
  p->u.func.apArg = (JsonNode **)xjd1PoolMallocZero(&pStmt->sPool, nByte);
  if( !p->u.func.apArg ){
    return XJD1_NOMEM;
  }

  return XJD1_OK;
}

static int aggExprStep(AggExpr *pAggExpr){
  Expr *p = pAggExpr->pExpr;
  Function *pFunc = p->u.func.pFunction;
  int i;
  int nItem = p->u.func.args->nEItem;

  assert( p->eType==TK_FUNCTION && p->eClass==XJD1_EXPR_FUNC );
  assert( pFunc->xStep && pFunc->xFinal && pFunc->xFunc==0 );

  for(i=0; i<nItem; i++){
    p->u.func.apArg[i] = xjd1ExprEval(p->u.func.args->apEItem[i].pExpr);
  }

  pFunc->xStep(nItem, p->u.func.apArg, &pAggExpr->pAggCtx);
  for(i=0; i<nItem; i++){
    xjd1JsonFree(p->u.func.apArg[i]);
  }

  return XJD1_OK;
}

int xjd1AggregateStep(Aggregate *pAgg){
  int i;                /* Used to iterate through aggregate functions */
  for(i=0; i<pAgg->nExpr; i++){
    int rc = aggExprStep(&pAgg->aAggExpr[i]);
    if( rc!=XJD1_OK ) return rc;
  }
  return XJD1_OK;;
}

int xjd1AggregateFinalize(Aggregate *pAgg){
  int i;
  for(i=0; i<pAgg->nExpr; i++){
    AggExpr *pAggExpr = &pAgg->aAggExpr[i];
    Expr *p = pAggExpr->pExpr;

    assert( i==p->u.func.iAgg );
    xjd1JsonFree(pAggExpr->pValue);
    pAggExpr->pValue = p->u.func.pFunction->xFinal(pAggExpr->pAggCtx);
    pAggExpr->pAggCtx = 0;
  }

  return XJD1_OK;
}

/*
** Call any outstanding xFinal() functions for aggregate functions in the
** query. This is required to reset the aggregate contexts when a query is 
** rewound following an error.
*/
void xjd1AggregateClear(Query *pQuery){
  Aggregate *pAgg = pQuery->pAgg;

  if( pAgg ){
    int i;
    for(i=0; i<pAgg->nExpr; i++){
      AggExpr *pAggExpr = &pAgg->aAggExpr[i];
      Expr *p = pAggExpr->pExpr;

      xjd1JsonFree( p->u.func.pFunction->xFinal(pAggExpr->pAggCtx) );
      xjd1JsonFree( pAggExpr->pValue );
      pAggExpr->pAggCtx = 0;
      pAggExpr->pValue = 0;
    }
  }
}

JsonNode *xjd1FunctionEval(Expr *p){
  JsonNode *pRet;
  Function *pFunc = p->u.func.pFunction;

  assert( p->eType==TK_FUNCTION && p->eClass==XJD1_EXPR_FUNC );

  if( pFunc->xFunc ){
    /* A scalar function. */
    int i;
    int nItem = p->u.func.args->nEItem;
    for(i=0; i<nItem; i++){
      p->u.func.apArg[i] = xjd1ExprEval(p->u.func.args->apEItem[i].pExpr);
    }

    pRet = pFunc->xFunc(nItem, p->u.func.apArg);
    for(i=0; i<nItem; i++){
      xjd1JsonFree(p->u.func.apArg[i]);
    }
  }else{
    AggExpr *pAggExpr = &p->pQuery->pAgg->aAggExpr[p->u.func.iAgg];
    pRet = xjd1JsonRef(pAggExpr->pValue);
  }

  return pRet;
}

