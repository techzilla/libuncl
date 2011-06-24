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
  p->pQuery = pQuery;
  switch( p->eDSType ){
    case TK_COMMA: {
      xjd1DataSrcInit(p->u.join.pLeft, pQuery);
      xjd1DataSrcInit(p->u.join.pRight, pQuery);
      break;
    }
    case TK_SELECT: {
      xjd1QueryInit(p->u.subq.q, pQuery->pStmt, pQuery);
      break;
    }
  }
  return XJD1_OK;
}

/*
** Rewind a data source so that it is pointing at the first row.
** Return XJD1_DONE if the data source is empty and XJD1_ROW if
** the rewind results in a row of content being available.
*/
int xjd1DataSrcRewind(DataSrc *p){
  return XJD1_DONE;
}

/*
** Advance a data source to the next row.
** Return XJD1_DONE if the data source is empty and XJD1_ROW if
** the step results in a row of content being available.
*/
int xjd1DataSrcStep(DataSrc *p){
  return XJD1_DONE;
}

/*
** Return true if the data source is at the end of file
*/
int xjd1DataSrcEOF(DataSrc *p){
  return 1;
}

/*
** The destructor for a Query object.
*/
int xjd1DataSrcClose(DataSrc *p){
  return XJD1_OK;
}
