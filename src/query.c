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
  }else{
    xjd1QueryRewind(p->u.compound.pLeft);
    p->u.compound.doneLeft = xjd1QueryEOF(p->u.compound.pLeft);
    xjd1QueryRewind(p->u.compound.pRight);
  }
  return XJD1_OK;
}

/*
** Advance a query to the next row.  Return XDJ1_DONE if there is no
** next row, or XJD1_ROW if the step was successful.
*/
int xjd1QueryStep(Query *p){
  int rc;
  if( p==0 ) return XJD1_DONE;
  if( p->eQType==TK_SELECT ){
    do{
      rc = xjd1DataSrcStep(p->u.simple.pFrom);
    }while(
         rc==XJD1_ROW
      && (p->u.simple.pWhere!=0 && !xjd1ExprTrue(p->u.simple.pWhere))
    );
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
      if( zDocName==0 && p->u.simple.pRes ){
        pOut = xjd1ExprEval(p->u.simple.pRes);
      }else{
        pOut = xjd1DataSrcDoc(p->u.simple.pFrom, zDocName);
      }
    }else if( !p->u.compound.doneLeft ){
      pOut = xjd1QueryDoc(p->u.compound.pLeft, zDocName);
    }else{
      pOut = xjd1QueryDoc(p->u.compound.pRight, zDocName);
    }
  }
  return pOut;
}


/* Return true if there are no more rows available on this query */
int xjd1QueryEOF(Query *p){
  int rc;
  if( p->eQType==TK_SELECT ){
    rc = xjd1DataSrcEOF(p->u.simple.pFrom);
  }else{
    rc = 0;
    if( !p->u.compound.doneLeft ){
      rc = xjd1QueryEOF(p->u.compound.pLeft);
      if( rc ) p->u.compound.doneLeft = 1;
    }
    if( p->u.compound.doneLeft ){
      rc = xjd1QueryEOF(p->u.compound.pRight);
    }
  }
  return rc;
}

/*
** The destructor for a Query object.
*/
int xjd1QueryClose(Query *pQuery){
  int rc = XJD1_OK;
  if( pQuery==0 ) return rc;
  if( pQuery->eQType==TK_SELECT ){
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
