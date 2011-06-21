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
** This file contains code used to scan a data source during a query.
*/
#include "xjd1Int.h"

int xjd1ScannerInit(Query *pQuery, DataSrc *pDSrc){
  if( pDSrc==0 ) return XJD1_OK;
  pDSrc->pQuery = pQuery;
  return XJD1_OK;
}
int xjd1ScannerEOF(DataSrc *pDSrc){
  return 1;
}
int xjd1ScannerStep(DataSrc *pDSrc){
  return XJD1_DONE;
}
int xjd1ScannerRewind(DataSrc *pDSrc){
  return XJD1_OK;
}
int xjd1ScannerClose(DataSrc *pDSrc){
  return XJD1_OK;
}
