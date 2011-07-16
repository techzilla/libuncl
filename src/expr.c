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
    case XJD1_EXPR_TRI: {
      walkExpr(p->u.tri.pTest, pAction);
      walkExpr(p->u.tri.pIfTrue, pAction);
      walkExpr(p->u.tri.pIfFalse, pAction);
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
  p->pQuery = pAction->pQuery;
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
  sAction.xNodeAction = walkInitCallback;
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
** Return non-zero if the JSON object passed should be considered TRUE in a 
** boolean context. For example in the result of a WHERE or HAVING clause.
**
** XJD1 uses the same rules as Javascript does to determine which
** values are considered TRUE:
**
**   1. Arrays and objects are always considered true.
**   2. Strings are false if they are zero bytes in length, otherwise true.
**   3. Numbers are false if equal to zero, or true otherwise.
**   4. NULL values are false.
**   5. "true" and "false" are "true" and "false". Respectively.
*/
static int isTrue(const JsonNode *p){
  int res = 0;                    /* Return value */
  switch( p->eJType ){
    case XJD1_REAL: 
      res = p->u.r!=0.0;
      break;

    case XJD1_STRING: 
      res = p->u.z[0]!='\0';
      break;

    case XJD1_ARRAY:
    case XJD1_STRUCT:
    case XJD1_TRUE:
      res = 1;
      break;
  }
  assert( res==1 || res==0 );
  return res;
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
** If the JSON node passed as the first argument is of type XJD1_STRUCT,
** attempt to return a pointer to property zProperty.
**
** If zProperty is not defined, or if pStruct is not of type XJD1_STRUCT,
** return a pointer to a NULL value.
*/
static JsonNode *getProperty(JsonNode *pStruct, const char *zProperty){
  JsonStructElem *pElem;
  JsonNode *pRes = 0;

  if( pStruct && pStruct->eJType==XJD1_STRUCT ){
    for(pElem=pStruct->u.st.pFirst; pElem; pElem=pElem->pNext){
      if( strcmp(pElem->zLabel, zProperty)==0 ){
        pRes = xjd1JsonRef(pElem->pValue);
        break;
      }
    }
  }

  if( pRes==0 ){
    pRes = nullJson();
  }
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
      pRes = getProperty(pBase, p->u.lvalue.zId);
      xjd1JsonFree(pBase);
      return pRes;
    }

    /* The x[y] operator. The result depends on the type of value x.
    **
    ** If x is of type XJD1_STRUCT, then expression y is converted to
    ** a string. The value returned is the value of property y of 
    ** object x.
    */
    case TK_LB: {
      pJLeft = xjd1ExprEval(p->u.bi.pLeft);
      pJRight = xjd1ExprEval(p->u.bi.pRight);

      switch( pJLeft->eJType ){
        case XJD1_STRUCT: {
          String idx;
          xjd1StringInit(&idx, 0, 0);
          xjd1JsonToString(pJRight, &idx);
          pRes = getProperty(pJLeft, idx.zBuf);
          xjd1StringClear(&idx);
          break;
        }

        case XJD1_ARRAY:
        case XJD1_STRING: {
          break;
        }

        default:
          pRes = nullJson();
          break;
      }

      xjd1JsonFree(pJLeft);
      xjd1JsonFree(pJRight);
      return pRes;
    }

    case TK_ID: {
      if( p->pQuery ){
        return xjd1QueryDoc(p->pQuery, p->u.id.zId);
      }else{
        return xjd1StmtDoc(p->pStmt, p->u.id.zId);
      }
    }

    /* The following two logical operators work in the same way as their
    ** javascript counterparts. i.e.
    **
    **    1. "x AND y" is equivalent to "x ? y : x"
    **    2. "x OR y" is equivalent to "x ? x : y"
    */
    case TK_AND:
    case TK_OR: {
      pJLeft = xjd1ExprEval(p->u.bi.pLeft);
      if( isTrue(pJLeft)==(p->eType==TK_OR) ){
        pRes = pJLeft;
      }else{
        xjd1JsonFree(pJLeft);
        pRes = xjd1ExprEval(p->u.bi.pRight);
      }
      return pRes;
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


    case TK_STAR:
    case TK_SLASH:
    case TK_MINUS: {
      pJLeft = xjd1ExprEval(p->u.bi.pLeft);
      xjd1JsonToReal(pJLeft, &rLeft);
      if( p->u.bi.pRight==0 ){
        assert( p->eType==TK_MINUS );
        pRes->u.r = -1.0 * rLeft;
      }else{
        pJRight = xjd1ExprEval(p->u.bi.pRight);
        xjd1JsonToReal(pJRight, &rRight);

        switch( p->eType ){
          case TK_MINUS: 
            pRes->u.r = rLeft-rRight; 
            break;

          case TK_SLASH: 
            if( rRight!=0.0 ){
              pRes->u.r = rLeft / rRight; 
            }
            break;

          case TK_STAR: 
            pRes->u.r = rLeft * rRight; 
            break;
        }
        xjd1JsonFree(pJRight);
      }
      pRes->eJType = XJD1_REAL;
      xjd1JsonFree(pJLeft);
      break;
    }

    case TK_BANG: {
      if( xjd1ExprTrue(p->u.bi.pLeft) ){
        pRes->eJType = XJD1_FALSE;
      }else{
        pRes->eJType = XJD1_TRUE;
      }
      break;
    }

    /* Bitwise operators: &, |, <<, >> and ~.
    **
    ** These follow the javascript conventions. Arguments are converted to
    ** 64-bit real numbers, and then to 32-bit signed integers. The bitwise
    ** operation is performed and the result converted back to a 64-bit
    ** real number.
    **
    ** TBD: When XJD1 is enhance to feature an arbitrary precision integer
    ** type, these will have to change somehow.
    **
    ** This block also contains the implementation of the modulo operator.
    ** As it requires the same 32-bit integer conversions as the bitwise
    ** operators.
    */
    case TK_REM:
    case TK_RSHIFT:
    case TK_LSHIFT:
    case TK_BITAND:
    case TK_BITOR: {
      int iLeft;
      int iRight;

      pJLeft = xjd1ExprEval(p->u.bi.pLeft);
      pJRight = xjd1ExprEval(p->u.bi.pRight);
      xjd1JsonToReal(pJLeft, &rLeft);
      xjd1JsonToReal(pJRight, &rRight);
      iLeft = rLeft;
      iRight = rRight;

      pRes->eJType = XJD1_REAL;
      switch( p->eType ){
        case TK_RSHIFT: 
          if( iRight>=32 ){
            pRes->u.r = (double)(iLeft<0 ? -1 : 0);
          }else{
            pRes->u.r = (double)(iLeft >> iRight);
          }
          break;

        case TK_LSHIFT:
          if( iRight>=32 ){
            pRes->u.r = 0;
          }else{
            pRes->u.r = (double)(iLeft << iRight);
          }
          break;

        case TK_BITAND: pRes->u.r = (double)(iLeft & iRight); break;
        case TK_BITOR:  pRes->u.r = (double)(iLeft | iRight); break;
        case TK_REM:    pRes->u.r = (double)(iLeft % iRight); break;
      }
      break;
    }

    case TK_BITNOT: {
      pJLeft = xjd1ExprEval(p->u.bi.pLeft);
      xjd1JsonToReal(pJLeft, &rLeft);
      pRes->eJType = XJD1_REAL;
      pRes->u.r = (double)(~((int)rLeft));
      break;
    }

    case TK_QM: {
      if( xjd1ExprTrue(p->u.tri.pTest) ){
        pRes = xjd1ExprEval(p->u.tri.pIfTrue);
      }else{
        pRes = xjd1ExprEval(p->u.tri.pIfFalse);
      }
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
** Return non-zero if the evaluation of the given expression should be
** considered TRUE in a boolean context. For example in result of a
** WHERE or HAVING clause.
*/
int xjd1ExprTrue(Expr *p){
  int rc = 0;
  JsonNode *pValue = xjd1ExprEval(p);
  if( pValue ){
    rc = isTrue(pValue);
    assert( rc==1 || rc==0 );
    xjd1JsonFree(pValue);
  }
  return rc;
}
