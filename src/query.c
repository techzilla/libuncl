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
int xjd1QueryInit(xjd1_stmt *pStmt, Query *pQuery){
  if( pQuery==0 ) return XJD1_OK;
  pQuery->pStmt = pStmt;
  if( pQuery->eQType==TK_SELECT ){
    xjd1ScannerInit(pQuery, pQuery->u.simple.pFrom);
  }else{
    xjd1QueryInit(pStmt, pQuery->u.compound.pLeft);
    xjd1QueryInit(pStmt, pQuery->u.compound.pRight);
  }
  return XJD1_OK;
}

/*
** Rewind a query so that it is pointing at the first row.
*/
int xjd1QueryRewind(Query *pQuery){
  if( pQuery==0 ) return XJD1_OK;
  if( pQuery->eQType==TK_SELECT ){
    xjd1ScannerRewind(pQuery->u.simple.pFrom);
  }else{
    xjd1QueryRewind(pQuery->u.compound.pLeft);
    xjd1QueryRewind(pQuery->u.compound.pRight);
  }
  return XJD1_OK;
}

/*
** Advance a query to the next row.
*/
int xjd1QueryStep(Query *pQuery){
  if( pQuery==0 ) return XJD1_OK;
  if( pQuery->eQType==TK_SELECT ){
    /* TBD */
  }else{
    /* TBD */
  }
  return XJD1_OK;
}

/*
** The destructor for a Query object.
*/
int xjd1QueryClose(Query *pQuery){
  int rc = XJD1_OK;
  if( pQuery==0 ) return rc;
  if( pQuery->eQType==TK_SELECT ){
    xjd1ScannerClose(pQuery->u.simple.pFrom);
  }else{
    xjd1QueryClose(pQuery->u.compound.pLeft);
    xjd1QueryClose(pQuery->u.compound.pRight);
  }
  return rc;
}
