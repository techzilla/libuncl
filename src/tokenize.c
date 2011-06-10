/*
** 2011 June 09
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** The tokenizer
*/
#include "xjd1Int.h"

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
/* Hash score: 51 */
static int keywordCode(const char *z, int n){
  /* zText[] encodes 271 bytes of keywords in 189 bytes */
  /*   BEGINSERTABLEACHAVINGROUPDATESCAPEXCEPTRUEXISTSELECTFALSEALL       */
  /*   IKEASCENDINGLOBETWEENOTCOLLATECOMMITCREATEDELETEDESCENDING         */
  /*   FLATTENULLIMITIFROMINTERSECTINTOFFSETORDEROLLBACKUNIONVALUE        */
  /*   WHEREBYDROP                                                        */
  static const char zText[188] = {
    'B','E','G','I','N','S','E','R','T','A','B','L','E','A','C','H','A','V',
    'I','N','G','R','O','U','P','D','A','T','E','S','C','A','P','E','X','C',
    'E','P','T','R','U','E','X','I','S','T','S','E','L','E','C','T','F','A',
    'L','S','E','A','L','L','I','K','E','A','S','C','E','N','D','I','N','G',
    'L','O','B','E','T','W','E','E','N','O','T','C','O','L','L','A','T','E',
    'C','O','M','M','I','T','C','R','E','A','T','E','D','E','L','E','T','E',
    'D','E','S','C','E','N','D','I','N','G','F','L','A','T','T','E','N','U',
    'L','L','I','M','I','T','I','F','R','O','M','I','N','T','E','R','S','E',
    'C','T','I','N','T','O','F','F','S','E','T','O','R','D','E','R','O','L',
    'L','B','A','C','K','U','N','I','O','N','V','A','L','U','E','W','H','E',
    'R','E','B','Y','D','R','O','P',
  };
  static const unsigned char aHash[80] = {
       0,  44,  42,  15,  45,  21,  43,   1,   0,   6,   3,  11,   0,
       7,   9,   0,   0,  40,   0,   5,  33,  30,  24,   0,   0,   0,
       0,  34,   0,   0,   0,  19,   0,   0,   0,  38,   0,   0,  13,
       0,   0,   0,   0,  41,   0,   0,   0,   0,   0,   0,   0,   0,
      23,  25,  37,  22,   4,  32,   0,   0,  29,  36,  17,  39,   0,
      35,  14,   0,   0,   0,   0,   0,  31,  26,   0,   0,   0,  27,
      20,  12,
  };
  static const unsigned char aNext[45] = {
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  10,   0,   0,
       0,   0,   0,   0,   0,   0,   2,   0,   0,   0,   0,   0,   0,
       8,   0,   0,   0,  18,   0,   0,  16,   0,   0,   0,   0,  28,
       0,   0,   0,   0,   0,   0,
  };
  static const unsigned char aLen[45] = {
       5,   6,   5,   4,   6,   5,   6,   6,   6,   4,   6,   2,   6,
       5,   3,   4,   3,   9,   2,   4,   7,   3,   7,   6,   6,   6,
       4,  10,   2,   7,   4,   5,   2,   4,   9,   4,   6,   3,   5,
       8,   5,   5,   5,   2,   4,
  };
  static const unsigned short int aOffset[45] = {
       0,   3,   8,  12,  15,  20,  23,  28,  33,  38,  41,  43,  46,
      52,  57,  59,  63,  63,  63,  71,  74,  80,  83,  90,  96, 102,
     108, 108, 115, 118, 124, 127, 132, 133, 137, 146, 149, 152, 155,
     159, 167, 172, 177, 182, 184,
  };
  static const unsigned char aCode[45] = {
    TK_BEGIN,      TK_INSERT,     TK_TABLE,      TK_FLATTENOP,  TK_HAVING,     
    TK_GROUP,      TK_UPDATE,     TK_ESCAPE,     TK_EXCEPT,     TK_TRUE,       
    TK_EXISTS,     TK_IS,         TK_SELECT,     TK_FALSE,      TK_ALL,        
    TK_LIKEOP,     TK_ASCENDING,  TK_ASCENDING,  TK_AS,         TK_LIKEOP,     
    TK_BETWEEN,    TK_NOT,        TK_COLLATE,    TK_COMMIT,     TK_CREATE,     
    TK_DELETE,     TK_DESCENDING, TK_DESCENDING, TK_IN,         TK_FLATTENOP,  
    TK_NULL,       TK_LIMIT,      TK_IF,         TK_FROM,       TK_INTERSECT,  
    TK_INTO,       TK_OFFSET,     TK_SET,        TK_ORDER,      TK_ROLLBACK,   
    TK_UNION,      TK_VALUE,      TK_WHERE,      TK_BY,         TK_DROP,       
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
#define XJD1_N_KEYWORD 45

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
      *tokenType = TK_EQ;
      return 1 + (z[1]=='=');
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
    case '[': {
      for(i=1, c=z[0]; c!=']' && (c=z[i])!=0; i++){}
      *tokenType = c==']' ? TK_ID : TK_ILLEGAL;
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
  xjd1_stmt *pStmt,
  const char *zCode,
  int *pN
){
  int i = 0;
  Parse sParse;
  int tokenType;
  void *pEngine;
  int lastTokenParsed = 0;
  int nErr = 0;
  extern void *xjd1ParserAlloc(void*(*)(size_t));
  extern void xjd1ParserFree(void*, void(*)(void*));
  extern void xjd1Parser(void*,int,Token,Parse*);

  *pN = 0;
  pEngine = xjd1ParserAlloc(malloc);
  if( pEngine==0 ) return XJD1_NOMEM;
  memset(&sParse, 0, sizeof(sParse));
  sParse.pPool = &pStmt->sPool;
  xjd1StringInit(&sParse.errMsg, &pStmt->sPool, 0);
  while( zCode[i] ){
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
        xjd1Parser(pEngine, tokenType, sParse.sTok, &sParse);
        lastTokenParsed = tokenType;
        if( sParse.errCode ) goto abort_parse;
        break;
      }
    }
  }
abort_parse:
  *pN = i;
  if( zCode[i]==0 && nErr==0 && sParse.errCode==XJD1_OK ){
    if( lastTokenParsed!=TK_SEMI ){
      xjd1Parser(pEngine, TK_SEMI, sParse.sTok, &sParse);
    }
    xjd1Parser(pEngine, 0, sParse.sTok, &sParse);
  }
  pStmt->pCmd = sParse.pCmd;
  xjd1ParserFree(pEngine, free);
  return nErr;
}
