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
};

#define ArraySize(X)    ((int)(sizeof(X)/sizeof(X[0])))

/*
** Implementation of "length(x)".
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

int xjd1FunctionInit(Expr *p, xjd1_stmt *pStmt){
  char *zName;
  int nArg;
  int nByte;
  int i;

  static Function aFunc[] = {
    { 1, "length", xLength },
  };

  zName = p->u.func.zFName;
  nArg = p->u.func.args->nEItem;

  for(i=0; i<ArraySize(aFunc); i++){
    Function *pFunc = &aFunc[i];
    if( strcmp(pFunc->zName, zName)==0 && nArg==pFunc->nArg ){
      p->u.func.pFunction = pFunc;
      break;
    }
  }

  if( !p->u.func.pFunction ){
    return XJD1_ERROR;
  }

  nByte = sizeof(JsonNode *) * nArg;
  p->u.func.apArg = (JsonNode **)xjd1PoolMallocZero(&pStmt->sPool, nByte);
  if( !p->u.func.apArg ){
    return XJD1_NOMEM;
  }

  return XJD1_OK;
}


JsonNode *xjd1FunctionEval(Expr *p){
  Function *pFunc = p->u.func.pFunction;
  int i;
  int nItem = p->u.func.args->nEItem;
  JsonNode *pRet;

  for(i=0; i<nItem; i++){
    p->u.func.apArg[i] = xjd1ExprEval(p->u.func.args->apEItem[i].pExpr);
  }

  pRet = pFunc->xFunc(nItem, p->u.func.apArg);
  for(i=0; i<nItem; i++){
    xjd1JsonFree(p->u.func.apArg[i]);
  }

  return pRet;
}

