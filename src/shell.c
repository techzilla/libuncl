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
** This file contains C code that implements a simple command-line
** wrapper shell around xjd1.
*/
#include "xjd1.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static void usage(const char *argv0){
  fprintf(stderr, "usage: %s DATABASE\n", argv0);
  exit(1);
}

int main(int argc, char **argv){
  xjd1 *pDb;
  int rc, n;
  char *zStmt = 0;
  int nStmt = 0;
  int nAlloc = 0;
  const char *zPrompt = "xjd1> ";
  char zLine[2000];

  if( argc!=2 ) usage(argv[0]);
  rc = xjd1_open(0, argv[1], &pDb);
  if( rc!=XJD1_OK ){
    fprintf(stderr, "cannot open \"%s\"\n", argv[1]);
    exit(1);
  }
  while( !feof(stdin) ){
    printf("%s", zPrompt);
    if( fgets(zLine, sizeof(zLine), stdin)==0 ) break;
    n = strlen(zLine);
    if( n+nStmt+1 >= nAlloc ){
      nAlloc = nAlloc + n + nStmt + 1000;
      zStmt = realloc(zStmt, nAlloc);
      if( zStmt==0 ){
        fprintf(stderr, "out of memory\n");
        exit(1);
      }
    }
    memcpy(&zStmt[nStmt], zLine, n+1);
    nStmt += n;
    if( xjd1_complete(zStmt) ){
      xjd1_stmt *pStmt;
      rc = xjd1_stmt_new(pDb, zStmt, &pStmt, &n);
      if( rc==XJD1_OK ){
        while( xjd1_stmt_step(pStmt)==XJD1_ROW ){
          const char *zVal;
          xjd1_stmt_value(pStmt, &zVal);
          printf("%s\n", zVal);
        }
      }
      xjd1_stmt_delete(pStmt);
      zPrompt = "xjd1> ";
    }else{
      zPrompt = " ...> ";
    }
  }
  xjd1_close(pDb);
  return 0;
}
