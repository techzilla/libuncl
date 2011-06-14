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
** This file contains code used to parse and render JSON strings.
*/
#include "xjd1Int.h"


/*
** Reclaim memory used by JsonNode objects 
*/
void xjd1JsonFree(JsonNode *p){
  if( p ){
    switch( p->eJType ){
      case XJD1_STRING: {
        free(p->u.z);
        break;
      }
      case XJD1_ARRAY: {
        int i;
        for(i=0; i<p->u.array.nElem; i++){
          xjd1JsonFree(p->u.array.apElem[i]);
        }
        free(p->u.array.apElem);
        break;
      }
      case XJD1_STRUCT: {
        JsonStructElem *pElem, *pNext;
        for(pElem=p->u.pStruct; pElem; pElem=pNext){
          pNext = pElem->pNext;
          free(pElem->zLabel);
          xjd1JsonFree(pElem->pValue);
          free(pElem);
        }
        break;
      }
    }
    free(p);
  }
}

/* Render a string as a string literal.
*/
void renderString(String *pOut, const char *z){
  int n, i, j, c;
  char *zOut;
  for(i=n=0; (c=z[i])!=0; i++, n++){
    if( c=='"' || c=='\\' ) n++;
  }
  xjd1StringAppend(pOut, 0, n+3);
  zOut = xjd1StringText(pOut);
  if( zOut ){
    zOut += xjd1StringLen(pOut);
    zOut[0] = '"';
    for(i=0, j=1; (c=z[i])!=0; i++){
      if( c=='"' || c=='\\' ) zOut[j++] = '\\';
      zOut[j++] = c;
    }
    zOut[j++] = '"';
    zOut[j] = 0;
    pOut->nUsed += j;
  }
}


/*
** Append the text for a JSON string.
*/
void xjd1JsonRender(String *pOut, JsonNode *p){
  if( p==0 ){
    xjd1StringAppend(pOut, "null", 4);
  }else{
    switch( p->eJType ){
      case XJD1_FALSE: {
        xjd1StringAppend(pOut, "false", 5);
        break;
      }
      case XJD1_TRUE: {
        xjd1StringAppend(pOut, "true", 4);
        break;
      }
      case XJD1_NULL: {
        xjd1StringAppend(pOut, "null", 4);
        break;
      }
      case XJD1_REAL: {
        xjd1StringAppendF(pOut, "%.17g", p->u.r);
        break;
      }
      case XJD1_STRING: {
        renderString(pOut, p->u.z);
        break;
      }
      case XJD1_ARRAY: {
        char cSep = '[';
        int i;
        for(i=0; i<p->u.array.nElem; i++){
          xjd1StringAppend(pOut, &cSep, 1);
          cSep = ',';
          xjd1JsonRender(pOut, p->u.array.apElem[i]);
        }
        xjd1StringAppend(pOut, "]", 1);
        break;
      }
      case XJD1_STRUCT: {
        char cSep = '{';
        JsonStructElem *pElem;
        for(pElem=p->u.pStruct; pElem; pElem=pElem->pNext){
          xjd1StringAppend(pOut, &cSep, 1);
          cSep = ',';
          renderString(pOut, pElem->zLabel);
          xjd1StringAppend(pOut, ",", 1);
          xjd1JsonRender(pOut, pElem->pValue);
        }
        xjd1StringAppend(pOut, "}", 1);
        break;
      }
    }
  }
}


/* JSON parser token types */
#define JSON_FALSE          XJD1_FALSE
#define JSON_TRUE           XJD1_TRUE
#define JSON_REAL           XJD1_REAL
#define JSON_NULL           XJD1_NULL
#define JSON_STRING         XJD1_STRING
#define JSON_BEGIN_ARRAY    XJD1_ARRAY
#define JSON_BEGIN_STRUCT   XJD1_STRUCT

#define JSON_END_ARRAY     20
#define JSON_COMMA         21
#define JSON_END_STRUCT    22
#define JSON_COLON         23
#define JSON_EOF           24
#define JSON_ERROR         25

/* State of a JSON string tokenizer */
typedef struct JsonStr JsonStr;
struct JsonStr {
  const char *zIn;        /* Complete input string */
  int iCur;               /* First character of current token */
  int n;                  /* Number of charaters in current token */
  int eType;              /* Type of current token */
};

/* Return the type of the current token */
#define tokenType(X)  ((X)->eType)

/* Return a pointer to the string of a token. */
#define tokenString(X) (&(X)->zIn[(X)->iCur])

/* Advance to the next token */
void tokenNext(JsonStr *p){
  int i, n;
  const char *z = p->zIn;
  i = p->n + p->iCur;
  while( xjd1Isspace(z[i]) ){ i++; }
  p->iCur = i;
  switch( z[i] ){
    case 0: {
      p->n = 0;
      p->eType = JSON_EOF;
      break;
    }
    case '"': {
      char c;
      for(n=1; (c = z[i+n])!=0 && c!='"'; n++){
        if( c=='\\' ) n++;
      }
      if( c=='"' ) n++;
      p->n = n;
      p->eType = JSON_STRING;
      break;
    }
    case '{': {
      p->n = 1;
      p->eType = JSON_BEGIN_STRUCT;
      break;
    }
    case '}': {
      p->n = 1;
      p->eType = JSON_END_STRUCT;
      break;
    }
    case '[': {
      p->n = 1;
      p->eType = JSON_BEGIN_ARRAY;
      break;
    }
    case ']': {
      p->n = 1;
      p->eType = JSON_END_ARRAY;
      break;
    }
    case ',': {
      p->n = 1;
      p->eType = JSON_COMMA;
      break;
    }
    case ':': {
      p->n = 1;
      p->eType = JSON_COLON;
      break;
    }
    case 't': {
      if( memcmp(&z[i],"true",4)==0 && xjd1Isident(z[i+4])==0 ){
        p->n = 4;
        p->eType = JSON_TRUE;
      }else{
        p->n = 1;
        p->eType = JSON_ERROR;
      }
      break;
    }
    case 'f': {
      if( memcmp(&z[i],"false",5)==0 && xjd1Isident(z[i+5])==0 ){
        p->n = 5;
        p->eType = JSON_FALSE;
      }else{
        p->n = 1;
        p->eType = JSON_ERROR;
      }
      break;
    }
    case 'n': {
      if( memcmp(&z[i],"null",4)==0 && xjd1Isident(z[i+4])==0 ){
        p->n = 4;
        p->eType = JSON_NULL;
      }else{
        p->n = 1;
        p->eType = JSON_ERROR;
      }
      break;
    }
    case '-':
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9': {
      n = 0;
      if( z[i]=='-' ){
        n = 1;
        if( !xjd1Isdigit(z[i+1]) ){
          p->n = 1;
          p->eType = JSON_ERROR;
          break;
        }
      }
      if( z[i+n]=='0' ){
        i++;
      }else{
        while( xjd1Isdigit(z[i+n]) ) n++;
      }
      if( z[i+n]=='.' && xjd1Isdigit(z[i+n+1]) ){
        n++;
        while( xjd1Isdigit(z[i+n]) ) n++;
      }
      if( (z[i+n]=='e' || z[i+n]=='E')
       && (((z[i+n+1]=='-' || z[i+n+1]=='+') && xjd1Isdigit(z[i+n+2]))
           || xjd1Isdigit(z[i+n+1]))
      ){
        n += 2;
        while( xjd1Isdigit(z[i+n]) ) n++;
      }
      p->n = n;
      p->eType = JSON_REAL;
      break;
    }
    default: {
      p->n = 1;
      p->eType = JSON_ERROR;
      break;
    }
  }
}

/* Convert the current token (which must be a string) into a true
** string (resolving all of the backslash escapes) and return a pointer
** to the true string.  Space is obtained form malloc().
*/
static char *tokenDequoteString(JsonStr *pIn){
  const char *zIn;
  char *zOut;
  int i, j, n;
  char c;
  zIn = &pIn->zIn[pIn->iCur];
  zOut = malloc( pIn->n );
  if( zOut==0 ) return 0;
  assert( zIn[0]=='"' && zIn[pIn->n-1]=='"' );
  n = pIn->n-1;
  for(i=1, j=0; i<n; i++){
    if( zIn[i]!='\\' ){
      zOut[j++] = zIn[i];
    }else{
      i++;
      c = zIn[i];
      if( c=='b' ){
        zOut[j++] = '\b';
      }else if( c=='f' ){
        zOut[j++] = '\f';
      }else if( c=='n' ){
        zOut[j++] = '\n';
      }else if( c=='r' ){
        zOut[j++] = '\r';
      }else if( c=='t' ){
        zOut[j++] = '\t';
      }else if( c=='u' && i<n-4 ){

      }else{
        zOut[j++] = c;
      }
    }
  }
  zOut[j] = 0;
  return zOut;
}


/* Enter point to the first token of the JSON object.
** Exit pointing to the first token past end end of the
** JSON object.
*/
static JsonNode *parseJson(JsonStr *pIn){
  JsonNode *pNew;
  pNew = malloc( sizeof(*pNew) );
  if( pNew==0 ) return 0;
  memset(pNew, 0, sizeof(*pNew));
  pNew->eJType = tokenType(pIn);
  switch( pNew->eJType ){
    case JSON_BEGIN_STRUCT: {
      JsonStructElem **ppTail;
      tokenNext(pIn);
      if( tokenType(pIn)==JSON_END_STRUCT ) break;
      ppTail = &pNew->u.pStruct;
      while( 1 ){
        JsonStructElem *pElem;
        if( tokenType(pIn)!=JSON_STRING ){
          goto json_error; 
        }
        pElem = malloc( sizeof(*pElem) );
        if( pElem==0 ) goto json_error;
        memset(pElem, 0, sizeof(*pElem));
        *ppTail = pElem;
        ppTail = &pElem->pNext;
        pElem->zLabel = tokenDequoteString(pIn);
        tokenNext(pIn);
        if( tokenType(pIn)!=JSON_COLON ){
          goto json_error;
        }
        tokenNext(pIn);
        pElem->pValue = parseJson(pIn);
        if( tokenType(pIn)==JSON_COMMA ){
          tokenNext(pIn);
        }else if( tokenType(pIn)==JSON_END_STRUCT ){
          tokenNext(pIn);
          break;
        }else{
          goto json_error;
        }
      }
      break;
    }
    case JSON_BEGIN_ARRAY: {
      int nAlloc = 0;
      tokenNext(pIn);
      if( tokenType(pIn)==JSON_END_ARRAY ) break;
      while( 1 ){
        if( pNew->u.array.nElem>=nAlloc ){
          JsonNode **pNewArray;
          nAlloc = nAlloc*2 + 5;
          pNewArray = realloc(pNew->u.array.apElem,
                              sizeof(JsonNode*)*nAlloc);
          if( pNewArray==0 ) goto json_error;
          pNew->u.array.apElem = pNewArray;
        }
        pNew->u.array.apElem[pNew->u.array.nElem++] = parseJson(pIn);
        if( tokenType(pIn)==JSON_COMMA ){
          tokenNext(pIn);
        }else if( tokenType(pIn)==JSON_END_ARRAY ){
          tokenNext(pIn);
          break;
        }else{
          goto json_error;
        }
      }     
      break;
    }
    case JSON_STRING: {
      pNew->u.z = tokenDequoteString(pIn);
      tokenNext(pIn);
      break;
    }
    case JSON_REAL: {
      pNew->u.r = atof(tokenString(pIn));
      tokenNext(pIn);
      break;
    }
    case JSON_TRUE:
    case JSON_FALSE:
    case JSON_NULL: {
      tokenNext(pIn);
      break;
    }
    default: {
      free(pNew);
      pNew = 0;
      break;
    }
  }
  return pNew;

json_error:
  free(pNew);
  return 0;
}

/*
** Parse up a JSON string
*/
JsonNode *xjd1JsonParse(const char *zIn){
  JsonStr x;
  x.zIn = zIn;
  x.iCur = 0;
  x.n = 0;
  x.eType = 0;
  tokenNext(&x);
  return parseJson(&x);
}
