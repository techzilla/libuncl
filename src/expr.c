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
    case XJD1_EXPR_Q: {
      if( pAction->xQueryAction ){
        rc = pAction->xQueryAction(p, pAction);
      }
      break;
    }
    case XJD1_EXPR_FUNC: {
      walkExprList(p->u.func.args, pAction);
      break;
    }
    case XJD1_EXPR_BI: {
      walkExpr(p->u.bi.pLeft, pAction);
      walkExpr(p->u.bi.pRight, pAction);
      break;
    }
    case XJD1_EXPR_TK: {
      /* Nothing to do */
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
    rc = xjd1QueryInit(p->u.q, pAction->pStmt, pAction->pQuery);
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
  sAction.xQueryAction = walkInitCallback;
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
  return xjd1QueryClose(p->u.q);
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
** Return a numeric value for a JsonNode.  Do whatever type
** conversions are necessary.
*/
static double realFromJson(JsonNode *pNode){
  double r = 0.0;
  if( pNode==0 ) return 0.0;
  switch( pNode->eJType ){
    case XJD1_TRUE: {
      r = 1.0;
      break;
    }
    case XJD1_REAL: {
      r = pNode->u.r;
      break;
    }
    case XJD1_STRING: {
      r = atof(pNode->u.z);
      break;
    }
  }
  return r;
}

/*
** Compute a real value from an expression
*/
static double realFromExpr(Expr *p){
  double r = 0.0;
  if( p==0 ) return 0.0;
  switch( p->eType ){
    case TK_STRING:
    case TK_INTEGER:
    case TK_FLOAT: {
      r = atof(p->u.tk.z);
      break;
    }
    case TK_TRUE: {
      r = 1.0;
      break;
    }
    case TK_FALSE:
    case TK_NULL: {
      break;
    }
    default: {
      JsonNode *pNode = xjd1ExprEval(p);
      r = realFromJson(pNode);
      xjd1JsonFree(pNode);
      break;
    }
  }
  return r;
}

/*
** Evaluate an expression.  Return the result as a JSON object.
**
** The caller must free the returned JSON by a call xjdJsonFree().
*/
JsonNode *xjd1ExprEval(Expr *p){
  JsonNode *pRes;
  double rLeft, rRight;
  if( p==0 ){
    pRes = xjd1JsonNew();
    if( pRes ) pRes->eJType = XJD1_NULL;
    return pRes;
  }
  if( p->eType==TK_JVALUE ){
    return xjd1JsonParse(p->u.tk.z, p->u.tk.n);
  }
  pRes = xjd1JsonNew();
  if( pRes==0 ) return 0;
  switch( p->eType ){
    case TK_INTEGER:
    case TK_FLOAT: {
      pRes->u.r = atof(p->u.tk.z);
      pRes->eJType = XJD1_REAL;
      break;
    }
    case TK_NULL: {
      pRes->eJType = XJD1_NULL;
      break;
    }
    case TK_TRUE: {
      pRes->eJType = XJD1_TRUE;
      break;
    }
    case TK_FALSE: {
      pRes->eJType = XJD1_FALSE;
      break;
    }
    case TK_STRING: {
      pRes->u.z = malloc( p->u.tk.n );
      if( pRes->u.z ){
        int i, j;
        for(i=1, j=0; i<p->u.tk.n-1; i++){
          char c = p->u.tk.z[i];
          if( c=='\\' ){
            c = p->u.tk.z[++i];
            if( c=='n' ){
              c = '\n';
            }else if( c=='t' ){
              c = '\t';
            }
          }
          pRes->u.z[++j] = c;
        }
        pRes->u.z[j] = 0;
      }
      pRes->eJType = XJD1_STRING;
      break;
    }
    case TK_PLUS: {
      rLeft = realFromExpr(p->u.bi.pLeft);
      rRight = realFromExpr(p->u.bi.pRight);
      pRes->eJType = XJD1_REAL;
      pRes->u.r = rLeft+rRight;  break;
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

/*
** Convert an ExprList into a JSON structure.
*/
JsonNode *xjd1ExprListEval(ExprList *pList){
  JsonNode *pRes;
  JsonStructElem *pElem, **ppLast;
  int i;
  ExprItem *pItem;

  pRes = xjd1JsonNew();
  if( pRes==0 ) return 0;
  pRes->eJType = XJD1_STRUCT;
  ppLast = &pRes->u.pStruct;
  if( pList==0 ) return pRes;
  for(i=0; i<pList->nEItem; i++){
    pItem = &pList->apEItem[i];
    pElem = malloc( sizeof(*pElem) );
    if( pElem==0 ) break;
    memset(pElem, 0, sizeof(*pElem));
    pElem->zLabel = malloc( pItem->tkAs.n+1 );
    if( pElem->zLabel ){
      memcpy(pElem->zLabel, pItem->tkAs.z, pItem->tkAs.n);
      pElem->zLabel[pItem->tkAs.n] = 0;
    }
    pElem->pValue = xjd1ExprEval(pItem->pExpr);
    *ppLast = pElem;
    ppLast = &pElem->pNext;
  }
  return pRes;  
}
