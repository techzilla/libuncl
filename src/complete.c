/*
** 2011-06-09
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** This file contains C code that implements the xjd1_complete() API.
*/
#include "xjd1Int.h"

/*
** Return true if the statement in zStmt is complete.
**
** A complete statement ends with a semicolon where the semicolon is
** not part of a larger token such as a string or a JSON structure.
**
** Trailing whitespace is ignored.  Comment text is considered whitespace
** for the purposes of the previous sentence.
**
** An empty string is considered a complete statement.
*/
int xjd1_complete(const char *zStmt){
  int isComplete = 1;
  if( zStmt==0 ) return 1;
  while( zStmt[0] ){
    switch( zStmt[0] ){
      case ';': {
        isComplete = 1;
        zStmt++;
        break;
      }
      case ' ':
      case '\r':
      case '\t':
      case '\n':
      case '\f': {  /* White space is ignored */
        zStmt++;
        break;
      }
      case '/': {   /* C-style comments */
        if( zStmt[1]!='*' ){
          token = tkOTHER;
          break;
        }
        zStmt += 2;
        while( zStmt[0] && (zStmt[0]!='*' || zStmt[1]!='/') ){ zStmt++; }
        if( zStmt[0]==0 ) return 0;
        zStmt++;
        break;
      }
      case '-': {   /* SQL-style comments from "--" to end of line */
        if( zStmt[1]!='-' ){
          zStmt++;
          break;
        }
        while( zStmt[0] && *zStmt!='\n' ){ zStmt++; }
        if( zStmt[0]==0 ) return isComplete;
        break;
      }
      case '"': {   /* C-style string literals */
        zStmt++;
        while( zStmt[0] && zStmt[0]!='"' ){
          if( zStmt[0]=='\\' ) zStmt++;
          zStmt++;
        }
        if( zStmt[0]==0 ) return 0;
        zStmt++;
        isComplete = 0;
        break;
      }
      default: {    /* Anything else */
        zStmt++;
        isComplete = 0;
        break;
      }
    }
  }
  return isComplete;
}
