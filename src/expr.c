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
** This file contains code used to evaluate expressions at run-time.
*/
#include "xjd1Int.h"
#include <math.h>

/*
** Information passed down into the expression walker.
*/
typedef struct WalkAction WalkAction;
struct WalkAction {
  int (*xQueryAction)(Expr*,WalkAction*);  /* Call for each EXPR_Q node */
  int (*xNodeAction)(Expr*,WalkAction*);   /* Call for every node */
  xjd1_stmt *pStmt;                        /* Stmt expr belongs to */
  Query *pQuery;                           /* Query expr belongs to */
  void *pArg;                              /* Some other argument */
};

/* forward reference */
static int walkExpr(Expr*,WalkAction*);

/*
** Walk an expression list 
*/
static int walkExprList(ExprList *p, WalkAction *pAction){
  if( p ){
    int i;
    for(i=0; i<p->nEItem; i++){
      walkExpr(p->apEItem[i].pExpr, pAction);
    }
  }
  return XJD1_OK;
}

/*
** Walk an expression tree
*/
static int walkExpr(Expr *p, WalkAction *pAction){
  int rc = XJD1_OK;
  if( p==0 ) return XJD1_OK;
  if( pAction->xNodeAction ){
    rc = pAction->xNodeAction(p, pAction);
  }
  switch( p->eClass ){
    case XJD1_EXPR_BI: {
      walkExpr(p->u.bi.pLeft, pAction);
      walkExpr(p->u.bi.pRight, pAction);
      break;
    }
    case XJD1_EXPR_TK: {
      /* Nothing to do */
      break;
    }
    case XJD1_EXPR_FUNC: {
      walkExprList(p->u.func.args, pAction);
      break;
    }
    case XJD1_EXPR_Q: {
      if( pAction->xQueryAction ){
        rc = pAction->xQueryAction(p, pAction);
      }
      break;
    }
    case XJD1_EXPR_JSON: {
      /* Nothing to do */
      break;
    }
    case XJD1_EXPR_ARRAY: {
      walkExprList(p->u.ar, pAction);
      break;
    }
    case XJD1_EXPR_STRUCT: {
      walkExprList(p->u.st, pAction);
      break;
    }
    case XJD1_EXPR_LVALUE: {
      walkExpr(p->u.lvalue.pLeft, pAction);
      break;
    }
  }
  return rc;
}


/*
** Callback for query expressions
*/
static int walkInitCallback(Expr *p, WalkAction *pAction){
  int rc = XJD1_OK;
  assert( p );
  p->pStmt = pAction->pStmt;
  if( p->eClass==XJD1_EXPR_Q ){
    rc = xjd1QueryInit(p->u.subq.p, pAction->pStmt, pAction->pQuery);
  }
  return rc;
}


/*
** Initialize an expression in preparation for evaluation of a
** statement.
*/
int xjd1ExprInit(Expr *p, xjd1_stmt *pStmt, Query *pQuery){
  WalkAction sAction;
  memset(&sAction, 0, sizeof(sAction));
  sAction.xNodeAction = walkInitCallback;
  sAction.pStmt = pStmt;
  sAction.pQuery = pQuery;
  return walkExpr(p, &sAction);
}

/*
** Initialize a list of expression in preparation for evaluation of a
** statement.
*/
int xjd1ExprListInit(ExprList *p, xjd1_stmt *pStmt, Query *pQuery){
  WalkAction sAction;
  memset(&sAction, 0, sizeof(sAction));
  sAction.xQueryAction = walkInitCallback;
  sAction.pStmt = pStmt;
  sAction.pQuery = pQuery;
  return walkExprList(p, &sAction);
}


/* Walker callback for ExprClose() */
static int walkCloseQueryCallback(Expr *p, WalkAction *pAction){
  assert( p );
  assert( p->eType==TK_SELECT );
  return xjd1QueryClose(p->u.subq.p);
}

/*
** Close all subqueries in an expression.
*/
int xjd1ExprClose(Expr *p){
  WalkAction sAction;
  memset(&sAction, 0, sizeof(sAction));
  sAction.xQueryAction = walkCloseQueryCallback;
  return walkExpr(p,&sAction);
}

/*
** Close all subqueries in an expression list.
*/
int xjd1ExprListClose(ExprList *p){
  WalkAction sAction;
  memset(&sAction, 0, sizeof(sAction));
  sAction.xQueryAction = walkCloseQueryCallback;
  return walkExprList(p,&sAction);
}

/*
** Return true if the JSON object is a string
*/
static int isStr(const JsonNode *p){
  if( p==0 ) return 0;
  switch( p->eJType ){
    case XJD1_TRUE:
    case XJD1_FALSE:
    case XJD1_NULL:
    case XJD1_REAL:
      return 0;
  }
  return 1;
}

/*
** Allocate a NULL JSON object.
*/
static JsonNode *nullJson(void){
  JsonNode *pRes = xjd1JsonNew(0);
  if( pRes ) pRes->eJType = XJD1_NULL;
  return pRes;
}

/*
** Evaluate an expression.  Return the result as a JSON object.
**
** The caller must free the returned JSON by a call xjdJsonFree().
*/
JsonNode *xjd1ExprEval(Expr *p){
  JsonNode *pRes;
  double rLeft, rRight;
  int c;
  JsonNode *pJLeft, *pJRight;

  if( p==0 ) return nullJson();
  switch( p->eType ){
    case TK_JVALUE: {
      return xjd1JsonRef(p->u.json.p);
    }
    case TK_DOT: {
      JsonNode *pBase = xjd1ExprEval(p->u.lvalue.pLeft);
      JsonStructElem *pElem;
      pRes = 0;
      if( pBase && pBase->eJType==XJD1_STRUCT ){
        for(pElem=pBase->u.st.pFirst; pElem; pElem=pElem->pNext){
          if( strcmp(pElem->zLabel, p->u.lvalue.zId)==0 ){
            pRes = xjd1JsonRef(pElem->pValue);
            break;
          }
        }
      }
      xjd1JsonFree(pBase);
      if( pRes==0 ) pRes = nullJson();
      return pRes;
    }
    case TK_LB: {
      return nullJson();   /* TBD */
    }
    case TK_ID: {
      return xjd1StmtDoc(p->pStmt, p->u.id.zId);
    }
  }
  pRes = xjd1JsonNew(0);
  if( pRes==0 ) return 0;
  pRes->eJType = XJD1_NULL;
  switch( p->eType ){
    case TK_STRUCT: {
      int i;
      JsonStructElem *pElem, **ppPrev;
      ExprList *pList = p->u.st;
      ppPrev = &pRes->u.st.pFirst;
      pRes->eJType = XJD1_STRUCT;
      for(i=0; i<pList->nEItem; i++){
        ExprItem *pItem = &pList->apEItem[i];
        pElem = malloc( sizeof(*pElem) );
        if( pElem==0 ) break;
        *ppPrev = pRes->u.st.pLast = pElem;
        ppPrev = &pElem->pNext;
        memset(pElem, 0, sizeof(*pElem));
        pElem->zLabel = xjd1PoolDup(0, pItem->zAs, -1);
        pElem->pValue = xjd1ExprEval(pItem->pExpr);
      }
      break;
    }
    case TK_ARRAY: {
      int i;
      ExprList *pList = p->u.st;
      pRes->u.ar.apElem = malloc( pList->nEItem*sizeof(JsonNode*) );
      if( pRes->u.ar.apElem ){
        pRes->u.ar.nElem = pList->nEItem;
        pRes->eJType = XJD1_ARRAY;
        for(i=0; i<pList->nEItem; i++){
          ExprItem *pItem = &pList->apEItem[i];
          pRes->u.ar.apElem[i] = xjd1ExprEval(pItem->pExpr);
        }
      }
      break;
    }
    case TK_EQEQ:
    case TK_NE:
    case TK_LT:
    case TK_LE:
    case TK_GT:
    case TK_GE: {
      pJLeft = xjd1ExprEval(p->u.bi.pLeft);
      pJRight = xjd1ExprEval(p->u.bi.pRight);
      c = xjd1JsonCompare(pJLeft, pJRight);
      xjd1JsonFree(pJLeft);
      xjd1JsonFree(pJRight);
      switch( p->eType ){
        case TK_EQEQ: c = c==0;   break;
        case TK_NE:   c = c!=0;   break;
        case TK_LT:   c = c<0;    break;
        case TK_LE:   c = c<=0;   break;
        case TK_GT:   c = c>0;    break;
        case TK_GE:   c = c>=0;   break;
      }
      pRes->eJType = c ? XJD1_TRUE : XJD1_FALSE;
      break;
    }      
    case TK_PLUS: {
      pJLeft = xjd1ExprEval(p->u.bi.pLeft);
      pJRight = xjd1ExprEval(p->u.bi.pRight);
      if( isStr(pJLeft) || isStr(pJRight) ){
        String x;
        xjd1StringInit(&x, 0, 0);
        xjd1JsonToString(pJLeft, &x);
        xjd1JsonToString(pJRight, &x);
        pRes->eJType = XJD1_STRING;
        pRes->u.z = xjd1StringGet(&x);
      }else{
        xjd1JsonToReal(pJLeft, &rLeft);
        xjd1JsonToReal(pJRight, &rRight);
        pRes->u.r = rLeft+rRight;
        pRes->eJType = XJD1_REAL;
      }
      xjd1JsonFree(pJLeft);
      xjd1JsonFree(pJRight);
      break;
    }
    case TK_MINUS: {
      xjd1JsonToReal(pJLeft, &rLeft);
      xjd1JsonToReal(pJRight, &rRight);
      pRes->u.r = rLeft-rRight;
      pRes->eJType = XJD1_REAL;
      break;
    }
    default: {
      pRes->eJType = XJD1_NULL;
      break;
    }
  }
  return pRes;
}


/*
** Return TRUE if the given expression evaluates to TRUE.
** An empty expression is considered to be TRUE.  A NULL value
** is not TRUE.
*/
int xjd1ExprTrue(Expr *p){
  int rc = 0;
  JsonNode *pValue = xjd1ExprEval(p);
  if( pValue==0 ) return 0;
  switch( pValue->eJType ){
    case XJD1_REAL: {
      rc = pValue->u.r!=0.0;
      break;
    }
    case XJD1_TRUE: {
      rc = 1;
      break;
    }
    case XJD1_STRING: {
      rc = atof(pValue->u.z)!=0.0;
      break;
    }
  }
  xjd1JsonFree(pValue);
  return rc;
}
