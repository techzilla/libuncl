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
** The tokenizer
*/
#include "xjd1Int.h"
#ifndef NDEBUG
#include <stdio.h>
#endif

/*
** The following 256 byte lookup table is used to support built-in
** equivalents to the following standard library functions:
**
**   isspace()                        0x01
**   isalpha()                        0x02
**   isdigit()                        0x04
**   isalnum()                        0x06
**   isxdigit()                       0x08
**   identifier character             0x40
**
** Bit 0x20 is set if the mapped character requires translation to upper
** case. i.e. if the character is a lower-case ASCII character.
** If x is a lower-case ASCII character, then its upper-case equivalent
** is (x - 0x20). Therefore toupper() can be implemented as:
**
**   (x & ~(map[x]&0x20))
**
** Bit 0x40 is set if the character non-alphanumeric and can be used in an 
** identifier.  Identifiers are alphanumerics, "_" and any
** non-ASCII UTF character. Hence the test for whether or not a character is
** part of an identifier is 0x46.
*/
const unsigned char xjd1CtypeMap[256] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* 00..07    ........ */
  0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00,  /* 08..0f    ........ */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* 10..17    ........ */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* 18..1f    ........ */
  0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* 20..27     !"#$%&' */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* 28..2f    ()*+,-./ */
  0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,  /* 30..37    01234567 */
  0x0c, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* 38..3f    89:;<=>? */

  0x00, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x02,  /* 40..47    @ABCDEFG */
  0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,  /* 48..4f    HIJKLMNO */
  0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,  /* 50..57    PQRSTUVW */
  0x02, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00, 0x40,  /* 58..5f    XYZ[\]^_ */
  0x00, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x22,  /* 60..67    `abcdefg */
  0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,  /* 68..6f    hijklmno */
  0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,  /* 70..77    pqrstuvw */
  0x22, 0x22, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00,  /* 78..7f    xyz{|}~. */

  0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  /* 80..87    ........ */
  0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  /* 88..8f    ........ */
  0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  /* 90..97    ........ */
  0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  /* 98..9f    ........ */
  0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  /* a0..a7    ........ */
  0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  /* a8..af    ........ */
  0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  /* b0..b7    ........ */
  0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  /* b8..bf    ........ */

  0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  /* c0..c7    ........ */
  0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  /* c8..cf    ........ */
  0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  /* d0..d7    ........ */
  0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  /* d8..df    ........ */
  0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  /* e0..e7    ........ */
  0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  /* e8..ef    ........ */
  0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  /* f0..f7    ........ */
  0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40   /* f8..ff    ........ */
};

/**********************************************************************
** The following code is automatically generated
** by ../tool/mkkeywordhash.c
*/
/* Hash score: 57 */
static int keywordCode(const char *z, int n){
  /* zText[] encodes 295 bytes of keywords in 204 bytes */
  /*   BEGINSERTRUELSELECTFALSEACHAVINGROUPDATESCAPEXCEPTALLIKEXISTS      */
  /*   ANDELETEASCENDINGLOBETWEENOTCOLLATECOLLECTIONULLIMITCOMMIT         */
  /*   CREATEDESCENDINGDROPRAGMAFLATTENIFROMINTERSECTINTOFFSETORDER       */
  /*   OLLBACKUNIONVALUEWHEREBY                                           */
  static const char zText[203] = {
    'B','E','G','I','N','S','E','R','T','R','U','E','L','S','E','L','E','C',
    'T','F','A','L','S','E','A','C','H','A','V','I','N','G','R','O','U','P',
    'D','A','T','E','S','C','A','P','E','X','C','E','P','T','A','L','L','I',
    'K','E','X','I','S','T','S','A','N','D','E','L','E','T','E','A','S','C',
    'E','N','D','I','N','G','L','O','B','E','T','W','E','E','N','O','T','C',
    'O','L','L','A','T','E','C','O','L','L','E','C','T','I','O','N','U','L',
    'L','I','M','I','T','C','O','M','M','I','T','C','R','E','A','T','E','D',
    'E','S','C','E','N','D','I','N','G','D','R','O','P','R','A','G','M','A',
    'F','L','A','T','T','E','N','I','F','R','O','M','I','N','T','E','R','S',
    'E','C','T','I','N','T','O','F','F','S','E','T','O','R','D','E','R','O',
    'L','L','B','A','C','K','U','N','I','O','N','V','A','L','U','E','W','H',
    'E','R','E','B','Y',
  };
  static const unsigned char aHash[80] = {
       0,  49,  47,  13,  34,  23,  48,   1,   0,   9,   0,  15,  26,
      10,  12,   0,   0,  45,   0,   8,  37,  36,  29,   0,   0,   0,
       0,  38,   0,   0,   0,  19,   0,   0,   0,  42,   0,   0,   5,
       0,   0,   0,   0,  46,   0,   0,   0,   0,   0,   0,   0,   0,
      25,  30,  41,  24,  43,  28,   0,  17,  33,  40,  20,  44,   0,
      39,   6,   0,   0,  35,   0,   0,  27,  18,   0,   0,   0,  31,
      22,  16,
  };
  static const unsigned char aNext[49] = {
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   3,   4,   0,   0,   0,   0,   0,   2,   0,   0,   0,   0,
      21,   0,   0,   0,  11,   0,   0,   0,   0,   0,   0,  14,   0,
       0,   0,   0,   7,  32,   0,   0,   0,   0,   0,
  };
  static const unsigned char aLen[49] = {
       5,   6,   4,   4,   6,   5,   4,   6,   5,   6,   6,   6,   3,
       4,   6,   2,   3,   6,   2,   3,   9,   4,   7,   3,   7,  10,
       4,   5,   6,   6,   4,  10,   2,   4,   6,   7,   2,   4,   9,
       4,   6,   3,   2,   5,   8,   5,   5,   5,   2,
  };
  static const unsigned short int aOffset[49] = {
       0,   3,   8,  11,  13,  19,  23,  26,  31,  34,  39,  44,  50,
      52,  55,  57,  61,  63,  69,  69,  69,  77,  80,  86,  89,  96,
     105, 108, 113, 119, 125, 125, 132, 135, 138, 144, 151, 152, 156,
     165, 168, 171, 174, 174, 178, 186, 191, 196, 201,
  };
  static const unsigned char aCode[49] = {
    TK_BEGIN,      TK_INSERT,     TK_TRUE,       TK_ELSE,       TK_SELECT,     
    TK_FALSE,      TK_FLATTENOP,  TK_HAVING,     TK_GROUP,      TK_UPDATE,     
    TK_ESCAPE,     TK_EXCEPT,     TK_ALL,        TK_LIKEOP,     TK_EXISTS,     
    TK_IS,         TK_AND,        TK_DELETE,     TK_AS,         TK_ASCENDING,  
    TK_ASCENDING,  TK_LIKEOP,     TK_BETWEEN,    TK_NOT,        TK_COLLATE,    
    TK_COLLECTION, TK_NULL,       TK_LIMIT,      TK_COMMIT,     TK_CREATE,     
    TK_DESCENDING, TK_DESCENDING, TK_IN,         TK_DROP,       TK_PRAGMA,     
    TK_FLATTENOP,  TK_IF,         TK_FROM,       TK_INTERSECT,  TK_INTO,       
    TK_OFFSET,     TK_SET,        TK_OR,         TK_ORDER,      TK_ROLLBACK,   
    TK_UNION,      TK_VALUE,      TK_WHERE,      TK_BY,         
  };
  int h, i;
  if( n<2 || z[0]<'A' || z[0]>'Z' ) return TK_ID;
  h = (z[0]*4 ^ z[n-1]*3 ^ n) % 80;
  for(i=((int)aHash[h])-1; i>=0; i=((int)aNext[i])-1){
    if( aLen[i]==n && memcmp(&zText[aOffset[i]],z,n)==0 ){
      return aCode[i];
    }
  }
  return TK_ID;
}
#define XJD1_N_KEYWORD 49

/* End of the automatically generated hash code
*********************************************************************/

/*
** Return the length of the token that begins at z[0]. 
** Store the token type in *tokenType before returning.
*/
int xjd1GetToken(const unsigned char *z, int *tokenType){
  int i, c;
  switch( *z ){
    case ' ': case '\t': case '\n': case '\f': case '\r': {
      for(i=1; xjd1Isspace(z[i]); i++){}
      *tokenType = TK_SPACE;
      return i;
    }
    case '-': {
      if( z[1]=='-' ){
        for(i=2; (c=z[i])!=0 && c!='\n'; i++){}
        *tokenType = TK_SPACE;
        return i;
      }
      *tokenType = TK_MINUS;
      return 1;
    }
    case '(': {
      *tokenType = TK_LP;
      return 1;
    }
    case ')': {
      *tokenType = TK_RP;
      return 1;
    }
    case '[': {
      *tokenType = TK_LB;
      return 1;
    }
    case ']': {
      *tokenType = TK_RB;
      return 1;
    }
    case '{': {
      *tokenType = TK_LC;
      return 1;
    }
    case '}': {
      *tokenType = TK_RC;
      return 1;
    }
    case ';': {
      *tokenType = TK_SEMI;
      return 1;
    }
    case '+': {
      *tokenType = TK_PLUS;
      return 1;
    }
    case '*': {
      *tokenType = TK_STAR;
      return 1;
    }
    case '/': {
      if( z[1]!='*' || z[2]==0 ){
        *tokenType = TK_SLASH;
        return 1;
      }
      for(i=3, c=z[2]; (c!='*' || z[i]!='/') && (c=z[i])!=0; i++){}
      if( c ) i++;
      *tokenType = TK_SPACE;
      return i;
    }
    case '%': {
      *tokenType = TK_REM;
      return 1;
    }
    case '=': {
      if( z[1]=='=' ){
        *tokenType = TK_EQEQ;
        return 2;
      }else{
        *tokenType = TK_EQ;
        return 1;
      }
    }
    case '<': {
      if( (c=z[1])=='=' ){
        *tokenType = TK_LE;
        return 2;
      }else if( c=='>' ){
        *tokenType = TK_NE;
        return 2;
      }else if( c=='<' ){
        *tokenType = TK_LSHIFT;
        return 2;
      }else{
        *tokenType = TK_LT;
        return 1;
      }
    }
    case '>': {
      if( (c=z[1])=='=' ){
        *tokenType = TK_GE;
        return 2;
      }else if( c=='>' ){
        *tokenType = TK_RSHIFT;
        return 2;
      }else{
        *tokenType = TK_GT;
        return 1;
      }
    }
    case '!': {
      if( z[1]!='=' ){
        *tokenType = TK_ILLEGAL;
        return 2;
      }else{
        *tokenType = TK_NE;
        return 2;
      }
    }
    case '|': {
      if( z[1]!='|' ){
        *tokenType = TK_BITOR;
        return 1;
      }else{
        *tokenType = TK_CONCAT;
        return 2;
      }
    }
    case ',': {
      *tokenType = TK_COMMA;
      return 1;
    }
    case ':': {
      *tokenType = TK_COLON;
      return 1;
    }
    case '&': {
      *tokenType = TK_BITAND;
      return 1;
    }
    case '~': {
      *tokenType = TK_BITNOT;
      return 1;
    }
    case '"': {
      int delim = z[0];
      for(i=1; (c=z[i])!=0; i++){
        if( c==delim ){
          if( z[i+1]==delim ){
            i++;
          }else{
            break;
          }
        }
      }
      *tokenType = TK_STRING;
      return i+1;
    }
    case '.': {
      if( !xjd1Isdigit(z[1]) ){
        *tokenType = TK_DOT;
        return 1;
      }
      /* If the next character is a digit, this is a floating point
      ** number that begins with ".".  Fall thru into the next case */
    }
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9': {
      *tokenType = TK_INTEGER;
      for(i=0; xjd1Isdigit(z[i]); i++){}
      if( z[i]=='.' ){
        i++;
        while( xjd1Isdigit(z[i]) ){ i++; }
        *tokenType = TK_FLOAT;
      }
      if( (z[i]=='e' || z[i]=='E') &&
           ( xjd1Isdigit(z[i+1]) 
            || ((z[i+1]=='+' || z[i+1]=='-') && xjd1Isdigit(z[i+2]))
           )
      ){
        i += 2;
        while( xjd1Isdigit(z[i]) ){ i++; }
        *tokenType = TK_FLOAT;
      }
      while( xjd1Isident(z[i]) ){
        *tokenType = TK_ILLEGAL;
        i++;
      }
      return i;
    }
    default: {
      if( !xjd1Isident(*z) ){
        break;
      }
      for(i=1; xjd1Isident(z[i]); i++){}
      *tokenType = keywordCode((char*)z, i);
      return i;
    }
  }
  *tokenType = TK_ILLEGAL;
  return 1;
}

/*
** Run the parser on the given SQL string.  The parser structure is
** passed in.  An SQLITE_ status code is returned.  If an error occurs
** then an and attempt is made to write an error message into 
** memory obtained from xjd1_malloc() and to make *pzErrMsg point to that
** error message.
*/
int xjd1RunParser(
  xjd1 *pConn,
  xjd1_stmt *pStmt,
  const char *zCode,
  int *pN
){
  int i = 0;
  Parse sParse;
  int tokenType;
  void *pEngine;
  int lastTokenParsed = 0;
  int nToken = 0;
  int nErr = 0;
  extern void *xjd1ParserAlloc(void*(*)(size_t));
  extern void xjd1ParserFree(void*, void(*)(void*));
  extern void xjd1Parser(void*,int,Token,Parse*);

#ifndef NDEBUG
  if( pConn->parserTrace ){
    extern void xjd1ParserTrace(FILE*, char*);
    xjd1ParserTrace(stdout, "parser> ");
  }
#endif
  *pN = 0;
  pEngine = xjd1ParserAlloc(malloc);
  if( pEngine==0 ) return XJD1_NOMEM;
  memset(&sParse, 0, sizeof(sParse));
  sParse.pConn = pConn;
  sParse.pPool = &pStmt->sPool;
  xjd1StringInit(&sParse.errMsg, &pStmt->sPool, 0);
  while( zCode[i] && sParse.errCode==0 ){
    assert( i>=0 );
    sParse.sTok.z = &zCode[i];
    sParse.sTok.n = xjd1GetToken((unsigned char*)&zCode[i], &tokenType);
    i += sParse.sTok.n;
    switch( tokenType ){
      case TK_SPACE: {
        break;
      }
      case TK_ILLEGAL: {
        xjd1StringTruncate(&sParse.errMsg);
        xjd1StringAppendF(&sParse.errMsg,
             "unrecognized token: \"%.*s\"",
             sParse.sTok.n, sParse.sTok.z);
        nErr++;
        goto abort_parse;
      }
      default: {
        nToken++;
        xjd1Parser(pEngine, tokenType, sParse.sTok, &sParse);
        lastTokenParsed = tokenType;
        if( sParse.errCode || tokenType==TK_SEMI ) goto abort_parse;
        break;
      }
    }
  }
abort_parse:
  *pN = i;
  if( sParse.errCode ) nErr++;
  if( nErr==0 && sParse.errCode==XJD1_OK && nToken>0 ){
    if( lastTokenParsed!=TK_SEMI ){
      sParse.sTok.z = ";";
      sParse.sTok.n = 1;
      xjd1Parser(pEngine, TK_SEMI, sParse.sTok, &sParse);
    }
    xjd1Parser(pEngine, 0, sParse.sTok, &sParse);
  }
  pStmt->pCmd = sParse.pCmd;
  xjd1ParserFree(pEngine, free);
  return nErr==0 && sParse.errCode==0
       && (sParse.pCmd!=0 || nToken==0) ? XJD1_OK : XJD1_ERROR;
}
